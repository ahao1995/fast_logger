#pragma once
#include <stdio.h>
#include <sys/types.h>

class file_appender
{
  public:
    explicit file_appender(const char *filename) : fp_(::fopen(filename, "ae")), written_bytes_(0)
    {
        ::setbuffer(fp_, buffer_, sizeof buffer_);
    }
    ~file_appender()
    {
        ::fclose(fp_);
    }

    void append(const char *logline, const size_t len)
    {
        size_t n      = write(logline, len);
        size_t remain = len - n;
        while (remain > 0) {
            size_t x = write(logline + n, remain);
            if (x == 0) {
                int err = ferror(fp_);
                if (err) {
                    fprintf(stderr, "file_appender::append() failed %d\n", err);
                }
                break;
            }
            n += x;
            remain = len - n; // remain -= x
        }
        written_bytes_ += len;
    }

    void flush()
    {
        ::fflush(fp_);
    }
    off_t writtenBytes() const
    {
        return written_bytes_;
    }

  private:
    size_t write(const char *logline, size_t len)
    {
        return ::fwrite_unlocked(logline, 1, len, fp_);
    }
    FILE *fp_;
    char  buffer_[64 * 1024];
    off_t written_bytes_;
};
