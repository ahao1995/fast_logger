#pragma once
#include <limits>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

namespace details {

enum param_type : int32_t
{
    // Indicates that there is a problem with the parameter
    INVALID = -6,

    // Indicates a dynamic width (i.e. the '*' in  %*.d)
    DYNAMIC_WIDTH = -5,

    // Indicates dynamic precision (i.e. the '*' in %.*d)
    DYNAMIC_PRECISION = -4,

    // Indicates that the parameter is not a string type (i.e. %d, %lf)
    NON_STRING = -3,

    // Indicates the parameter is a string and has a dynamic precision
    // (i.e. '%.*s' )
    STRING_WITH_DYNAMIC_PRECISION = -2,

    // Indicates a string with no precision specified (i.e. '%s' )
    STRING_WITH_NO_PRECISION = -1,

    // All non-negative values indicate a string with a precision equal to its
    // enum value casted as an int32_t
    STRING = 0
};

#define FASTLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check) __attribute__((__format__(__printf__, string_index, first_to_check)))

/**
 * No-Op function that triggers the GNU preprocessor's format checker for
 * printf format strings and argument parameters.
 *
 * \param format
 *      printf format string
 * \param ...
 *      format parameters
 */
static void FASTLOG_PRINTF_FORMAT_ATTR(1, 2) check_format(const char *, ...)
{
}

constexpr inline bool is_terminal(char c)
{
    return c == 'd' || c == 'i' || c == 'u' || c == 'o' || c == 'x' || c == 'X' || c == 'f' || c == 'F' || c == 'e' || c == 'E' || c == 'g' ||
           c == 'G' || c == 'a' || c == 'A' || c == 'c' || c == 'p' || c == '%' || c == 's' || c == 'n';
}

/**
 * Checks whether a character is in the set of characters that specifies
 * a flag according to the printf specification:
 * http://www.cplusplus.com/reference/cstdio/printf/
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is in the set
 */
constexpr inline bool is_flag(char c)
{
    return c == '-' || c == '+' || c == ' ' || c == '#' || c == '0';
}

/**
 * Checks whether a character is in the set of characters that specifies
 * a length field according to the printf specification:
 * http://www.cplusplus.com/reference/cstdio/printf/
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is in the set
 */
constexpr inline bool is_length(char c)
{
    return c == 'h' || c == 'l' || c == 'j' || c == 'z' || c == 't' || c == 'L';
}

/**
 * Checks whether a character is a digit (0-9) or not.
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is a digit
 */
constexpr inline bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

template <int N>
constexpr inline param_type get_param_Info(const char (&fmt)[N], int paramNum = 0)
{
    int pos = 0;
    while (pos < N - 1) {
        // The code below searches for something that looks like a printf
        // specifier (i.e. something that follows the format of
        // %<flags><width>.<precision><length><terminal>). We only care
        // about precision and type, so everything else is ignored.
        if (fmt[pos] != '%') {
            ++pos;
            continue;
        } else {
            // Note: gcc++ 5,6,7,8 seems to hang whenever one uses the construct
            // "if (...) {... continue; }" without an else in constexpr
            // functions. Hence, we have the code here wrapped in an else {...}
            // I reported this bug to the developers here
            // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86767
            ++pos;

            // Two %'s in a row => Comment
            if (fmt[pos] == '%') {
                ++pos;
                continue;
            } else {
                // Consume flags
                while (is_flag(fmt[pos]))
                    ++pos;

                // Consume width
                if (fmt[pos] == '*') {
                    if (paramNum == 0)
                        return param_type::DYNAMIC_WIDTH;

                    --paramNum;
                    ++pos;
                } else {
                    while (is_digit(fmt[pos]))
                        ++pos;
                }

                // Consume precision
                bool hasDynamicPrecision = false;
                int  precision           = -1;
                if (fmt[pos] == '.') {
                    ++pos; // consume '.'

                    if (fmt[pos] == '*') {
                        if (paramNum == 0)
                            return param_type::DYNAMIC_PRECISION;

                        hasDynamicPrecision = true;
                        --paramNum;
                        ++pos;
                    } else {
                        precision = 0;
                        while (is_digit(fmt[pos])) {
                            precision = 10 * precision + (fmt[pos] - '0');
                            ++pos;
                        }
                    }
                }

                // consume length
                while (is_length(fmt[pos]))
                    ++pos;

                // Consume terminal
                if (!is_terminal(fmt[pos])) {
                    throw std::invalid_argument("Unrecognized format specifier after %");
                }

                // Fail on %n specifiers (i.e. store position to address) since
                // we cannot know the position without formatting.
                if (fmt[pos] == 'n') {
                    throw std::invalid_argument("%n specifiers are not support in NanoLog!");
                }

                if (paramNum != 0) {
                    --paramNum;
                    ++pos;
                    continue;
                } else {
                    if (fmt[pos] != 's')
                        return param_type::NON_STRING;

                    if (hasDynamicPrecision)
                        return param_type::STRING_WITH_DYNAMIC_PRECISION;

                    if (precision == -1)
                        return param_type::STRING_WITH_NO_PRECISION;
                    else
                        return param_type(precision);
                }
            }
        }
    }

    return param_type::INVALID;
}

template <int N>
constexpr inline int count_fmt_params(const char (&fmt)[N])
{
    int count = 0;
    while (get_param_Info(fmt, count) != param_type::INVALID)
        ++count;
    return count;
}

template <typename T>
inline
typename std::enable_if<!std::is_same<T, const wchar_t *>::value && !std::is_same<T, const char *>::value && !std::is_same<T, wchar_t *>::value &&
                        !std::is_same<T, char *>::value && !std::is_same<T, const void *>::value && !std::is_same<T, void *>::value,
                        size_t>::type
get_arg_size(size_t &stringSize, T arg)
{
    return sizeof(T) + sizeof(uint32_t);
}

inline size_t get_arg_size(size_t &stringSize, const void *)
{
    return sizeof(void *) + sizeof(uint32_t);
}

inline size_t get_arg_size(size_t &stringBytes, const char *str)
{
    stringBytes = strlen(str);
    return stringBytes + sizeof(uint32_t);
}
template <int argNum = 0, int M>
inline size_t get_arg_sizes(size_t (&)[M])
{
    return 0;
}
template <int argNum = 0, int M, typename T1, typename... Ts>
inline size_t get_arg_sizes(size_t (&stringSizes)[M], T1 head, Ts... rest)
{
    return get_arg_size(stringSizes[argNum], head) + get_arg_sizes<argNum + 1>(stringSizes, rest...);
}

template <typename T>
inline typename std::enable_if<!std::is_same<T, const wchar_t *>::value && !std::is_same<T, const char *>::value &&
                               !std::is_same<T, wchar_t *>::value && !std::is_same<T, char *>::value,
                               void>::type
store_argument(char **storage, T arg, size_t stringSize)
{
    auto size = sizeof(T);
    memcpy(*storage, &size, sizeof(uint32_t));
    *storage += sizeof(uint32_t);

    memcpy(*storage, &arg, sizeof(T));
    *storage += sizeof(T);
    return;
}

// string specialization of the above
template <typename T>
inline typename std::enable_if<std::is_same<T, const char *>::value || std::is_same<T, char *>::value, void>::type
store_argument(char **storage, T arg, const size_t stringSize)
{
    if (stringSize > std::numeric_limits<uint32_t>::max()) {
        throw std::invalid_argument("Strings larger than std::numeric_limits<uint32_t>::max() are unsupported");
    }
    auto size = static_cast<uint32_t>(stringSize);
    memcpy(*storage, &size, sizeof(uint32_t));
    *storage += sizeof(uint32_t);

    memcpy(*storage, arg, stringSize);
    *storage += stringSize;
    return;
}

template <int argNum = 0, int M>
inline void store_arguments(size_t (&stringSizes)[M], char **)
{
}
template <int argNum = 0, int M, typename T1, typename... Ts>
inline void store_arguments(size_t (&stringBytes)[M], char **storage, T1 head, Ts... rest)
{
    // Peel off one argument to store, and then recursively process rest
    store_argument(storage, head, stringBytes[argNum]);
    store_arguments<argNum + 1>(stringBytes, storage, rest...);
}
} // namespace details