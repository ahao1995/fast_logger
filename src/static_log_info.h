#pragma once
#include "level.h"
#include <iostream>
#include <regex>
#include <stdint.h>
#include <string.h>

struct print_fragment
{
    // The type of the argument to pull from the dynamic buffer to the
    // partial format string (format_fragment)
    uint8_t arg_type : 5;

    // Indicates that the fragment requires a dynamic width/precision
    // argument in addition to one required by the format specifier.
    bool has_dynamic_width : 1;
    bool has_dynamic_precision : 1;

    // TODO(syang0) is this necessary? The format fragment is null-terminated
    // Length of the format fragment
    uint16_t fragment_length;

    // A fragment of the original LOG statement that contains at most
    // one format specifier.
    char format_fragment[];
};
enum format_type : uint8_t
{
    NONE,

    unsigned_char_t,
    unsigned_short_int_t,
    unsigned_int_t,
    unsigned_long_int_t,
    unsigned_long_long_int_t,
    uintmax_t_t,
    size_t_t,
    wint_t_t,

    signed_char_t,
    short_int_t,
    int_t,
    long_int_t,
    long_long_int_t,
    intmax_t_t,
    ptrdiff_t_t,

    double_t,
    long_double_t,
    const_void_ptr_t,
    const_char_ptr_t,
    const_wchar_t_ptr_t,

    MAX_FORMAT_TYPE
};

struct static_log_info
{
    constexpr static_log_info(const uint32_t line_num, log_level lelvel, const char *fmt_str, uint8_t args_num)
        : line_num(line_num), lelvel(lelvel), fmt_str(fmt_str), args_num(args_num)
    {
    }
    bool            create_log_fragments(char **microCode);
    const uint32_t  line_num;
    const log_level lelvel;
    const char     *fmt_str;
    uint8_t         args_num;
    uint8_t         num_print_fragments{0};
    char           *fragments{nullptr};
};
