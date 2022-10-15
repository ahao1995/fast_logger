#pragma once
#include "file_appender.h"
#include "level.h"
#include "log_handler.h"
#include "staging_buffer.h"
#include "static_log_info.h"
#include "tscns.h"
#include "utils.h"
#include <algorithm>
#include <assert.h>
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
    inline void log(int &log_id, const int linenum, const log_level level, const char (&format)[M], int n_params, Ts... args)
    {
        assert(n_params == static_cast<uint32_t>(sizeof...(Ts)));
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
        header->userdata       = log_id;
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
    void                          ensure_staging_buffer_allocated();
    staging_buffer::queue_header *alloc(size_t size);
    void                          finish();

    void register_log(static_log_info &info, int &log_id);
    void adjust_heap(size_t i);
    void poll_inner();

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

#define FAST_LOG(level, format, ...)                                                                                                                 \
    do {                                                                                                                                             \
        static int    log_id   = UNASSIGNED_LOGID;                                                                                                   \
        constexpr int n_params = details::count_fmt_params(format);                                                                                  \
        if (false) {                                                                                                                                 \
            details::check_format(format, ##__VA_ARGS__);                                                                                            \
        }                                                                                                                                            \
        fast_logger::get_logger().log(log_id, __LINE__, level, format, n_params, ##__VA_ARGS__);                                                     \
    } while (0)

#define POLL()                                                                                                                                       \
    do {                                                                                                                                             \
        fast_logger::get_logger().poll();                                                                                                            \
    } while (0)
