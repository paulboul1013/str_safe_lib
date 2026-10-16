#ifndef STR_SAFE_LIB_STRING_H
#define STR_SAFE_LIB_STRING_H

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>

#if !defined(__GNUC__) && !defined(__clang__)
#error "strscpy_auto requires GCC or Clang compile-time type support"
#endif

#define STRSAFE_SAME_TYPE(a, b) \
    __builtin_types_compatible_p(__typeof__(a), __typeof__(b))

/*
 * Contributes zero to a size expression, but rejects pointers at compile
 * time.  This prevents sizeof(pointer) from masquerading as buffer capacity.
 */
#define STRSAFE_MUST_BE_ARRAY(a)                                            \
    (0UL * sizeof(struct {                                                  \
         _Static_assert(!STRSAFE_SAME_TYPE((a), &(a)[0]),                  \
                        "destination must be an array, not a pointer");    \
         unsigned char strsafe_array_check;                                \
     }))

void *memset(void *s, int c, size_t count);
size_t strnlen(const char *s, size_t maxlen);

/*
 * src and dst must not overlap.  src must be readable through its first NUL
 * byte or through size bytes, and dst must provide size writable bytes.
 * strscpy always terminates when size is nonzero; -E2BIG means truncation.
 */
ssize_t strscpy(char *dst, const char *src, size_t size);
ssize_t strscpy_pad(char *dst, const char *src, size_t size);

/* memcpy and memcpy_pad require non-overlapping readable/writable ranges. */
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memcpy_pad(void *dest, size_t dest_len, const void *src, size_t src_len,
                 int pad);

/* The strtomem outputs are fixed-width byte fields, not C strings. */
void strtomem(void *dest, size_t dest_len, const char *src);
void strtomem_pad(void *dest, size_t dest_len, const char *src, int pad);

#define strscpy_auto(dst, src)                                              \
    strscpy((dst), (src), sizeof(dst) + STRSAFE_MUST_BE_ARRAY(dst))

#define strtomem_auto(dst, src)                                             \
    strtomem((dst), sizeof(dst) + STRSAFE_MUST_BE_ARRAY(dst), (src))

#define strtomem_pad_auto(dst, src, pad)                                    \
    strtomem_pad((dst), sizeof(dst) + STRSAFE_MUST_BE_ARRAY(dst), (src),    \
                 (pad))

#endif
