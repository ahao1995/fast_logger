#pragma once
#include "spsc_var_queue_opt.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
class staging_buffer
{
  public:
    using queue_header = SPSCVarQueueOPT<1024 * 1024 * 4>::MsgHeader;
    staging_buffer() : varq_()
    {
        uint32_t tid = static_cast<pid_t>(syscall(SYS_gettid));
        snprintf(name_, sizeof(name_), "%d", tid);
    }
    ~staging_buffer()
    {
    }
    inline queue_header *alloc(size_t nbytes)
    {
        return varq_.alloc(nbytes);
    }
    inline void finish()
    {
        varq_.push();
    }
    queue_header *front()
    {
        return varq_.front();
    }
    inline void pop()
    {
        varq_.pop();
    }
    void set_name(const char *name)
    {
        strncpy(name_, name, sizeof(name) - 1);
    }
    const char *get_name()
    {
        return name_;
    }

    bool check_can_delete()
    {
        return should_deallocate_;
    }

    void set_delete_flag()
    {
        should_deallocate_ = true;
    }

  private:
    SPSCVarQueueOPT<1024 * 1024 * 4> varq_;
    char                             name_[16] = {0};
    bool                             should_deallocate_{false};
};