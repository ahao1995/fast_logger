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

class stra_logger
{
  public:
    stra_logger(TSCNS &tscns) : tscns_(tscns), log_handler_(tscns_)
    {
        tscns_.init();
    }
    ~stra_logger()
    {
        if (staging_buffer_) {
            delete staging_buffer_;
        }
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
        size_t   alloc_size                      = details::get_arg_sizes(string_sizes, args...) + sizeof(uint64_t);
        auto     header                          = alloc(alloc_size);
        if (!header) {
            std::cout << "header is null" << std::endl;
            return;
        }
        header->userdata      = log_id;
        char *writePos        = (char *)(header + 1);
        *(uint64_t *)writePos = timestamp;
        writePos += 8;
        details::store_arguments(string_sizes, &writePos, args...);
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
    /////////////////////////////////////////////////////////////////
    void ensure_staging_buffer_allocated()
    {
        if (staging_buffer_ == nullptr) {
            staging_buffer_ = new staging_buffer();
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
        std::lock_guard<std::mutex> lock(register_mutex);
        if (log_id != UNASSIGNED_LOGID)
            return;
        char *p = static_cast<char *>(malloc(1024));
        info.create_log_fragments(&p);
        log_id = static_cast<int32_t>(log_infos_.size()) + static_cast<int32_t>(log_handler_.get_log_infos_cnt());
        log_infos_.emplace_back(info);
    }

    void poll_inner()
    {
        if (log_infos_.size()) {
            std::unique_lock<std::mutex> lock(register_mutex);
            for (auto info : log_infos_) {
                log_handler_.add_log_info(info);
            }
            log_infos_.clear();
        }

        while (true) {
            if (staging_buffer_ == nullptr) {
                ensure_staging_buffer_allocated();
            }
            auto h = staging_buffer_->front();
            if (h == nullptr || h->userdata >= log_handler_.get_log_infos_cnt())
                break;
            log_handler_.handle_log(staging_buffer_->get_name(), h);
            staging_buffer_->pop();
        }
    }

  private:
    std::vector<static_log_info> log_infos_;

  private:
    log_level       cur_level_{log_level::TRACE};
    staging_buffer *staging_buffer_{nullptr};
    std::mutex      register_mutex;
    TSCNS          &tscns_;
    log_handler     log_handler_;
};
#define STRA_LOG(stra_logger, level, format, ...)                                                                                                    \
    do {                                                                                                                                             \
        static int log_id = UNASSIGNED_LOGID;                                                                                                        \
        stra_logger->log(log_id, __LINE__, level, format, ##__VA_ARGS__);                                                                            \
    } while (0)
