#pragma once
#include <limits>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <type_traits>

namespace details {
template <typename T>
inline
typename std::enable_if<!std::is_same<T, const wchar_t *>::value && !std::is_same<T, const char *>::value && !std::is_same<T, wchar_t *>::value &&
                        !std::is_same<T, char *>::value && !std::is_same<T, const void *>::value && !std::is_same<T, void *>::value,
                        size_t>::type
get_arg_size(size_t &stringSize, T arg)
{
    return sizeof(T);
}

inline size_t get_arg_size(size_t &stringSize, const void *)
{
    return sizeof(void *);
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
