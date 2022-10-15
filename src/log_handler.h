#pragma once
#include "file_appender.h"
#include "staging_buffer.h"
#include "static_log_info.h"
#include "tscns.h"
#include <iostream>
#include <vector>

class log_handler
{
  public:
    log_handler(TSCNS &tscns) : tscns_(tscns)
    {
    }
    ~log_handler()
    {
        if (file_) {
            file_->flush();
            delete file_;
        }
        for (auto info : bg_log_infos_) {
            if (info.fragments) {
                delete info.fragments;
            }
        }
    }
    void set_log_file(const char *filename)
    {
        if (file_ != nullptr)
            return;
        file_ = new file_appender(filename);
    }
    size_t get_log_infos_cnt()
    {
        return bg_log_infos_.size();
    }
    void add_log_info(static_log_info info)
    {
        bg_log_infos_.push_back(info);
    }
    template <typename T>
    int handle_single_arg(char *output, const char *formatString, T arg, uint32_t data_size)
    {
        if (data_size != sizeof(T)) {
            return sprintf(output, " [error arg] ");
        }
        return sprintf(output, formatString, arg);
    }
    void handle_log(const char *name, const staging_buffer::queue_header *header)
    {
        char             output_buf[1024 * 1024];
        static_log_info &info = bg_log_infos_[header->userdata];
        header++;
        uint64_t timestamp = *(uint64_t *)(header);
        header++;
        char              *argData           = (char *)header;
        static const char *log_level_names[] = {"TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
        const char        *log_level         = log_level_names[info.lelvel];
        uint64_t           ns                = tscns_.tsc2ns(timestamp);
        time_t             seconds           = static_cast<time_t>(ns / (1000000000));
        int                muns              = static_cast<int>(ns % (1000000000));
        int                ms                = muns / 1000000;
        int                us                = (muns % 1000000) / 1000;
        if (seconds != last_second_) {
            last_second_ = seconds;
            struct tm tm_time;
            {
                ::localtime_r(&seconds, &tm_time);
            }
            snprintf(time_buf_, sizeof(time_buf_), "%04d-%02d-%02d %02d:%02d:%02d", tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
                     tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
        }
        char *output  = output_buf;
        int   log_len = 0;
        log_len += sprintf(output, "[%s] [%s %03d.%03d] [%s] ", name, time_buf_, ms, us, log_level);
        output             = output_buf + log_len;
        print_fragment *pf = reinterpret_cast<print_fragment *>(info.fragments);
        for (int i = 0; i < info.num_print_fragments; ++i) {
            uint32_t data_size = *(uint32_t *)(argData);
            argData += sizeof(uint32_t);
            switch (pf->arg_type) {
                case NONE:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
                    log_len += sprintf(output, pf->format_fragment);
#pragma GCC diagnostic pop
                    break;

                case unsigned_char_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(unsigned char *)argData, data_size);
                    break;
                case unsigned_short_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(unsigned short int *)argData, data_size);
                    break;
                case unsigned_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(unsigned int *)argData, data_size);
                    break;
                case unsigned_long_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(unsigned long int *)argData, data_size);
                    break;
                case unsigned_long_long_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(unsigned long long int *)argData, data_size);
                    break;
                case uintmax_t_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(uintmax_t *)argData, data_size);
                    break;
                case size_t_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(size_t *)argData, data_size);
                    break;
                case wint_t_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(wint_t *)argData, data_size);
                    break;
                case signed_char_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(signed char *)argData, data_size);
                    break;
                case short_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(short int *)argData, data_size);
                    break;
                case int_t: {
                    log_len += handle_single_arg(output, pf->format_fragment, *(int *)argData, data_size);
                    break;
                }
                case long_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(long int *)argData, data_size);
                    break;
                case long_long_int_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(long long int *)argData, data_size);
                    break;
                case intmax_t_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(intmax_t *)argData, data_size);
                    break;
                case ptrdiff_t_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(ptrdiff_t *)argData, data_size);
                    break;
                case double_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(double *)argData, data_size);
                    break;
                case long_double_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(long double *)argData, data_size);
                    break;
                case const_void_ptr_t:
                    log_len += handle_single_arg(output, pf->format_fragment, *(const void **)argData, data_size);
                    break;
                case const_char_ptr_t: {
                    char buf[data_size + 1];
                    memcpy(buf, argData, data_size);
                    buf[data_size] = '\0';
                    log_len += sprintf(output, pf->format_fragment, buf);
                    break;
                }
                case MAX_FORMAT_TYPE:
                default:
                    log_len += sprintf(output, "Error: Corrupt log header in header file\r\n");
                    exit(-1);
            }
            output = output_buf + log_len;
            argData += data_size;
            pf = reinterpret_cast<print_fragment *>(reinterpret_cast<char *>(pf) + pf->fragment_length + sizeof(print_fragment));
        }
        if (file_) {
            sprintf(output, "\n");
            file_->append(output_buf, log_len + 1);
        } else {
            fprintf(stdout, "%s\n", output_buf);
        }
    }

  private:
    file_appender               *file_{nullptr};
    std::vector<static_log_info> bg_log_infos_;
    TSCNS                       &tscns_;
    char                         time_buf_[64];
    time_t                       last_second_{0};
};
