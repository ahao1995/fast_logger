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
inline format_type get_format_type(std::string length, char specifier)
{
    // Signed Integers
    if (specifier == 'd' || specifier == 'i') {
        if (length.empty())
            return format_type::int_t;

        if (length.size() == 2) {
            if (length[0] == 'h')
                return format_type::signed_char_t;
            if (length[0] == 'l')
                return format_type::long_long_int_t;
        }

        switch (length[0]) {
            case 'h':
                return format_type::short_int_t;
            case 'l':
                return format_type::long_int_t;
            case 'j':
                return format_type::intmax_t_t;
            case 'z':
                return format_type::size_t_t;
            case 't':
                return format_type::ptrdiff_t_t;
            default:
                break;
        }
    }

    // Unsigned integers
    if (specifier == 'u' || specifier == 'o' || specifier == 'x' || specifier == 'X') {
        if (length.empty())
            return format_type::unsigned_int_t;

        if (length.size() == 2) {
            if (length[0] == 'h')
                return format_type::unsigned_char_t;
            if (length[0] == 'l')
                return format_type::unsigned_long_long_int_t;
        }

        switch (length[0]) {
            case 'h':
                return format_type::unsigned_short_int_t;
            case 'l':
                return format_type::unsigned_long_int_t;
            case 'j':
                return format_type::uintmax_t_t;
            case 'z':
                return format_type::size_t_t;
            case 't':
                return format_type::ptrdiff_t_t;
            default:
                break;
        }
    }

    // Strings
    if (specifier == 's') {
        if (length.empty())
            return format_type::const_char_ptr_t;
        if (length[0] == 'l')
            return format_type::const_wchar_t_ptr_t;
    }

    // Pointer
    if (specifier == 'p') {
        if (length.empty())
            return format_type::const_void_ptr_t;
    }

    // Floating points
    if (specifier == 'f' || specifier == 'F' || specifier == 'e' || specifier == 'E' || specifier == 'g' || specifier == 'G' || specifier == 'a' ||
        specifier == 'A') {
        if (length.size() == 1 && length[0] == 'L')
            return format_type::long_double_t;
        else
            return format_type::double_t;
    }

    if (specifier == 'c') {
        if (length.empty())
            return format_type::int_t;
        if (length[0] == 'l')
            return format_type::wint_t_t;
    }

    fprintf(stderr, "Attempt to decode format specifier failed: %s%c\r\n", length.c_str(), specifier);
    return format_type::MAX_FORMAT_TYPE;
}

struct static_log_info
{
    constexpr static_log_info(const uint32_t line_num, log_level lelvel, const char *fmt_str, uint8_t args_num)
        : line_num(line_num), lelvel(lelvel), fmt_str(fmt_str), args_num(args_num)
    {
    }
    bool            create_fragments(char **microCode);
    const uint32_t  line_num;
    const log_level lelvel;
    const char     *fmt_str;
    uint8_t         args_num;
    uint8_t         num_print_fragments{0};
    char           *fragments{nullptr};
};

bool static_log_info::create_fragments(char **microCode)
{
    char *micro_code_starting_pos = *microCode;
    fragments                     = micro_code_starting_pos;
    size_t format_str_length      = strlen(fmt_str) + 1;

    size_t            i = 0;
    std::cmatch       match;
    int               consecutive_percents   = 0;
    size_t            start_of_next_fragment = 0;
    print_fragment   *pf                     = nullptr;
    static std::regex regex("^%"
                            "([-+ #0]+)?"            // Flags (Position 1)
                            "([\\d]+|\\*)?"          // Width (Position 2)
                            "(\\.(\\d+|\\*))?"       // Precision (Position 4; 3 includes '.')
                            "(hh|h|l|ll|j|z|Z|t|L)?" // Length (Position 5)
                            "([diuoxXfFeEgGaAcspn])" // Specifier (Position 6)
    );
    // The key idea here is to split up the format string in to fragments (i.e.
    // PrintFragments) such that there is at most one specifier per fragment.
    // This then allows the decompressor later to consume one argument at a
    // time and print the fragment (vs. buffering all the arguments first).

    while (i < format_str_length) {
        char c = fmt_str[i];
        // Skip the next character if there's an escape
        if (c == '\\') {
            i += 2;
            continue;
        }

        if (c != '%') {
            ++i;
            consecutive_percents = 0;
            continue;
        }

        // If there's an even number of '%'s, then it's a comment
        if (++consecutive_percents % 2 == 0 || !std::regex_search(fmt_str + i, match, regex)) {
            ++i;
            continue;
        }

        // Advance the pointer to the end of the specifier & reset the % counter
        consecutive_percents = 0;
        i += match.length();

        // At this point we found a match, let's start analyzing it
        pf = reinterpret_cast<print_fragment *>(*microCode);
        *microCode += sizeof(print_fragment);

        std::string width     = match[2].str();
        std::string precision = match[4].str();
        std::string length    = match[5].str();
        char        specifier = match[6].str()[0];
        format_type type      = get_format_type(length, specifier);
        if (type == MAX_FORMAT_TYPE) {
            fprintf(stderr, "Error: Couldn't process this: %s\r\n", match.str().c_str());
            *microCode = micro_code_starting_pos;
            return false;
        }

        pf->arg_type              = 0x1F & type;
        pf->has_dynamic_width     = (width.empty()) ? false : width[0] == '*';
        pf->has_dynamic_precision = (precision.empty()) ? false : precision[0] == '*';
        // Tricky tricky: We null-terminate the fragment by copying 1
        // extra byte and then setting it to NULL
        pf->fragment_length = static_cast<uint16_t>(i - start_of_next_fragment + 1);
        memcpy(*microCode, fmt_str + start_of_next_fragment, pf->fragment_length);
        *microCode += pf->fragment_length;
        *(*microCode - 1) = '\0';

        start_of_next_fragment = i;
        ++num_print_fragments;
    }

    // If we didn't encounter any specifiers, make one for a basic string
    if (pf == nullptr) {
        pf = reinterpret_cast<print_fragment *>(*microCode);
        *microCode += sizeof(print_fragment);
        num_print_fragments = 1;

        pf->arg_type          = format_type::NONE;
        pf->has_dynamic_width = pf->has_dynamic_precision = false;
        pf->fragment_length                               = static_cast<uint16_t>(format_str_length);
        memcpy(*microCode, fmt_str, format_str_length);
        *microCode += format_str_length;
    } else {
        // Extend the last fragment to include the rest of the string
        size_t endingLength = format_str_length - start_of_next_fragment;
        memcpy(pf->format_fragment + pf->fragment_length - 1, // -1 to erase \0
               fmt_str + start_of_next_fragment, endingLength);
        pf->fragment_length = static_cast<uint16_t>(pf->fragment_length - 1 + endingLength);
        *microCode += endingLength;
    }
    return true;
}