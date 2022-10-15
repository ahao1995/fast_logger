#pragma once
#include <stdint.h>
static constexpr int UNASSIGNED_LOGID = -1;
enum log_level : uint8_t
{
    TRACE = 0,
    DEBUG = 1,
    INFO  = 2,
    WARN  = 3,
    ERROR = 4,
    FATAL = 5,
    NUM_LOG_LEVELS
};
