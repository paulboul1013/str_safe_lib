#include "String.h"

#include <stdint.h>

/*
 * GCC can otherwise turn a replacement memset loop into a call to memset,
 * recursively calling this function.  Clang's no_builtin attribute provides
 * the corresponding per-function guarantee.
 */
#if defined(__clang__)
#define STRSAFE_NO_BUILTIN(name) __attribute__((no_builtin(name)))
#elif defined(__GNUC__)
#define STRSAFE_NO_BUILTIN(name)                                           \
    __attribute__((optimize("no-tree-loop-distribute-patterns")))
#else
#define STRSAFE_NO_BUILTIN(name)
#endif

STRSAFE_NO_BUILTIN("memset")
void *memset(void *s, int c, size_t count)
{
    unsigned char *destination = s;

    while (count-- != 0U) {
        *destination++ = (unsigned char)c;
    }

    return s;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t length;

    for (length = 0U; length < maxlen && s[length] != '\0'; ++length) {
    }

    return length;
}

ssize_t strscpy(char *dst, const char *src, size_t size)
{
    size_t i;

    if (size == 0 || size > INT_MAX) {
        return -E2BIG;
    }

    for (i = 0U; i < size - 1U; ++i) {
        dst[i] = src[i];

        if (src[i] == '\0') {
            return (ssize_t)i;
        }
    }

    dst[i] = '\0';

    return src[i] ? -E2BIG : (ssize_t)i;
}

ssize_t strscpy_pad(char *dst, const char *src, size_t size)
{
    ssize_t ret;

    ret = strscpy(dst, src, size);

    if (ret >= 0) {
        size_t copied = (size_t)ret;

        memset(dst + copied + 1U, '\0', size - copied - 1U);
    }

    return ret;
}

STRSAFE_NO_BUILTIN("memcpy")
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *destination = dest;
    const unsigned char *source = src;

    while (n-- != 0U) {
        *destination++ = *source++;
    }

    return dest;
}

STRSAFE_NO_BUILTIN("memmove")
void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *d = dest;
    const uint8_t *s = src;

    /* Unsigned subtraction handles both address orderings without undefined
     * relational comparisons between pointers to different objects. */
    if ((uintptr_t)d - (uintptr_t)s >= n) {
        while (n-- != 0U) {
            *d++ = *s++;
        }
    } else {
        while (n != 0U) {
            --n;
            d[n] = s[n];
        }
    }

    return dest;
}

void *memcpy_pad(void *dest, size_t dest_len, const void *src, size_t src_len,
                 int pad)
{
    size_t copy_len = src_len < dest_len ? src_len : dest_len;

    memcpy(dest, src, copy_len);

    if (copy_len < dest_len) {
        memset((unsigned char *)dest + copy_len, pad, dest_len - copy_len);
    }

    return dest;
}

void strtomem(void *dest, size_t dest_len, const char *src)
{
    size_t len = strnlen(src, dest_len);

    memcpy(dest, src, len);
}

void strtomem_pad(void *dest, size_t dest_len, const char *src, int pad)
{
    size_t len = strnlen(src, dest_len);

    memcpy_pad(dest, dest_len, src, len, pad);
}
