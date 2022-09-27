#pragma once
#include "file_appender.h"
#include "level.h"
#include "log_handler.h"
#include "staging_buffer.h"
#include "static_log_info.h"
#include "tscns.h"
#include "utils.h"
#include <algorithm>
#include <iostream>
#include <mutex>
#include <stdint.h>
#include <string.h>
#include <vector>

class fast_logger
{
  public:
    static fast_logger &get_logger()
    {
        static fast_logger logger;
        return logger;
    }
    ~fast_logger()
    {
    }
    template <int M, typename... Ts>
    inline void log(int &log_id, const int linenum, const log_level level, const char (&format)[M], Ts... args)
    {
        if (static_cast<int>(level) < static_cast<int>(cur_level_)) {
            return;
        }
        if (log_id == UNASSIGNED_LOGID) {
            static_log_info info(linenum, level, format, static_cast<uint8_t>(sizeof...(args)));
            register_log(info, log_id);
        }
        uint64_t timestamp                       = tscns_.rdtsc();
        size_t   string_sizes[sizeof...(Ts) + 1] = {}; // HACK: Zero length arrays are not allowed
        size_t   alloc_size                      = details::get_arg_sizes(string_sizes, args...) + sizeof(timestamp);
        auto     header                          = alloc(alloc_size);
        if (!header)
            return;
        header->userdata      = log_id;
        char *write_pos        = (char *)(header + 1);
        *(uint64_t *)write_pos = timestamp;
        write_pos += sizeof(timestamp);
        details::store_arguments(string_sizes, &write_pos, args...);
        finish();
    }

    void set_log_file(const char *filename)
    {
        log_handler_.set_log_file(filename);
    }

    void set_log_level(log_level logLevel)
    {
        cur_level_ = logLevel;
    }

    log_level get_log_level()
    {
        return cur_level_;
    }
    void poll()
    {
        poll_inner();
    }

  private:
    fast_logger() : log_handler_(tscns_)
    {
        tscns_.init();
    }

  private:
    /////////////////////////////////////////////////////////////////
    void ensure_staging_buffer_allocated()
    {
        if (staging_buffer_ == nullptr) {
            std::unique_lock<std::mutex> guard(buffer_mutex_);
            guard.unlock();
            staging_buffer_ = new staging_buffer();
            // sbc_.staging_buffer_created();
            guard.lock();
            thread_buffers_.push_back(staging_buffer_);
        }
    }

    staging_buffer::queue_header *alloc(size_t size)
    {
        if (staging_buffer_ == nullptr) {
            ensure_staging_buffer_allocated();
        }
        return staging_buffer_->alloc(size);
    }

    void finish()
    {
        staging_buffer_->finish();
    }

  private:
    void register_log(static_log_info &info, int &log_id)
    {
        std::lock_guard<std::mutex> lock(register_mutex_);
        if (log_id != UNASSIGNED_LOGID)
            return;
        char *p = static_cast<char *>(malloc(1024));
        info.create_fragments(&p);
        log_id = static_cast<int32_t>(log_infos_.size()) + static_cast<int32_t>(log_handler_.get_log_infos_cnt());
        log_infos_.push_back(info);
    }

    void adjust_heap(size_t i)
    {
        while (true) {
            size_t min_i = i;
            size_t ch    = std::min(i * 2 + 1, bg_thread_buffers_.size());
            size_t end   = std::min(ch + 2, bg_thread_buffers_.size());
            for (; ch < end; ch++) {
                auto h_ch  = bg_thread_buffers_[ch].header;
                auto h_min = bg_thread_buffers_[min_i].header;
                if (h_ch && (!h_min || *(int64_t *)(h_ch + 1) < *(int64_t *)(h_min + 1)))
                    min_i = ch;
            }
            if (min_i == i)
                break;
            std::swap(bg_thread_buffers_[i], bg_thread_buffers_[min_i]);
            i = min_i;
        }
    }

    void poll_inner()
    {
        int64_t tsc = tscns_.rdtsc();
        if (log_infos_.size()) {
            std::unique_lock<std::mutex> lock(register_mutex_);
            for (auto info : log_infos_) {
                log_handler_.add_log_info(info);
            }
            log_infos_.clear();
        }
        if (thread_buffers_.size()) {
            std::unique_lock<std::mutex> lock(buffer_mutex_);
            for (auto tb : thread_buffers_) {
                bg_thread_buffers_.emplace_back(tb);
            }
            thread_buffers_.clear();
        }
        for (size_t i = 0; i < bg_thread_buffers_.size(); i++) {
            auto &node = bg_thread_buffers_[i];
            if (node.header)
                continue;
            node.header = node.tb->front();
            if (!node.header && node.tb->check_can_delete()) {
                delete node.tb;
                node = bg_thread_buffers_.back();
                bg_thread_buffers_.pop_back();
                i--;
            }
        }
        if (bg_thread_buffers_.empty())
            return;

        // build heap
        for (int i = bg_thread_buffers_.size() / 2; i >= 0; i--) {
            adjust_heap(i);
        }
        while (true) {
            auto h = bg_thread_buffers_[0].header;
            if (!h || h->userdata >= log_handler_.get_log_infos_cnt() || *(int64_t *)(h + 1) >= tsc)
                break;
            auto tb = bg_thread_buffers_[0].tb;
            log_handler_.handle_log(tb->get_name(), h);
            tb->pop();
            bg_thread_buffers_[0].header = tb->front();
            adjust_heap(0);
        }
    }

  private:
    class staging_buffer_destroyer
    {
      public:
        explicit staging_buffer_destroyer()
        {
        }
        void staging_buffer_created()
        {
        }
        virtual ~staging_buffer_destroyer()
        {
            if (fast_logger::staging_buffer_ != nullptr) {
                staging_buffer_->set_delete_flag();
                fast_logger::staging_buffer_ = nullptr;
            }
        }
        friend class fast_logger;
    };

  private:
    struct heap_node
    {
        heap_node(staging_buffer *buffer) : tb(buffer)
        {
        }

        staging_buffer                     *tb;
        const staging_buffer::queue_header *header{nullptr};
    };
    std::vector<heap_node> bg_thread_buffers_;

  private:
    log_level                                    cur_level_{log_level::TRACE};
    std::vector<static_log_info>                 log_infos_;
    static thread_local staging_buffer          *staging_buffer_;
    static thread_local staging_buffer_destroyer sbc_;
    std::vector<staging_buffer *>                thread_buffers_;
    std::mutex                                   register_mutex_;
    std::mutex                                   buffer_mutex_;
    TSCNS                                        tscns_;
    log_handler                                  log_handler_;
};

thread_local staging_buffer                       *fast_logger::staging_buffer_ = nullptr;
thread_local fast_logger::staging_buffer_destroyer fast_logger::sbc_;

#define FAST_LOG(level, format, ...)                                                                                                                 \
    do {                                                                                                                                             \
        static int log_id = UNASSIGNED_LOGID;                                                                                                        \
        fast_logger::get_logger().log(log_id, __LINE__, level, format, ##__VA_ARGS__);                                                               \
    } while (0)

#define POLL()                                                                                                                                       \
    do {                                                                                                                                             \
        fast_logger::get_logger().poll()                                                                                                             \
    } while (0)
