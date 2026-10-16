#include "../String.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Calls go through volatile function pointers so the compiler cannot replace
 * calls to the library's libc-named functions with compiler builtins.  Compile
 * String.c with -fno-builtin as well, especially when optimization is enabled.
 */
static void *(*volatile call_memset)(void *, int, size_t) = memset;
static size_t (*volatile call_strnlen)(const char *, size_t) = strnlen;
static ssize_t (*volatile call_strscpy)(char *, const char *, size_t) = strscpy;
static ssize_t (*volatile call_strscpy_pad)(char *, const char *, size_t) =
    strscpy_pad;
static void *(*volatile call_memcpy)(void *, const void *, size_t) = memcpy;
static void *(*volatile call_memmove)(void *, const void *, size_t) = memmove;
static void *(*volatile call_memcpy_pad)(void *, size_t, const void *, size_t,
                                         int) = memcpy_pad;
static void (*volatile call_strtomem)(void *, size_t, const char *) = strtomem;
static void (*volatile call_strtomem_pad)(void *, size_t, const char *, int) =
    strtomem_pad;

static unsigned int checks;
static unsigned int failures;

static int bytes_equal(const unsigned char *actual,
                       const unsigned char *expected, size_t length)
{
    size_t i;

    for (i = 0; i < length; ++i) {
        if (actual[i] != expected[i]) {
            return 0;
        }
    }
    return 1;
}

static void check_true(int condition, const char *expression, const char *test,
                       int line)
{
    ++checks;
    if (!condition) {
        ++failures;
        (void)fprintf(stderr, "FAIL %s:%d: %s\n", test, line, expression);
    }
}

#define CHECK(expression) \
    check_true((expression), #expression, __func__, __LINE__)

#define CHECK_BYTES(actual, expected)                                      \
    check_true(bytes_equal((const unsigned char *)(actual),                 \
                           (const unsigned char *)(expected),               \
                           sizeof(expected)),                               \
               #actual " has expected bytes", __func__, __LINE__)

static void test_memset_edges_and_canaries(void)
{
    unsigned char zero_length[] = {0xa1, 0xa2, 0xa3};
    const unsigned char zero_expected[] = {0xa1, 0xa2, 0xa3};
    unsigned char region[] = {0xc1, 0x10, 0x20, 0x30, 0x40, 0xc2};
    const unsigned char region_expected[] = {0xc1, 0xab, 0xab,
                                              0xab, 0xab, 0xc2};
    void *ret;

    ret = call_memset(zero_length + 1, 0, 0);
    CHECK(ret == zero_length + 1);
    CHECK_BYTES(zero_length, zero_expected);

    ret = call_memset(region + 1, 0x1ab, 4);
    CHECK(ret == region + 1);
    CHECK_BYTES(region, region_expected);
}

static void test_strnlen_bounds(void)
{
    const char terminated[] = {'a', 'b', 'c', '\0', 'x'};
    const char unterminated[] = {'a', 'b', 'c', 'd'};
    const char empty[] = {'\0', 'x'};

    CHECK(call_strnlen(terminated, 0) == 0);
    CHECK(call_strnlen(empty, 2) == 0);
    CHECK(call_strnlen(terminated, 5) == 3);
    CHECK(call_strnlen(terminated, 3) == 3);
    CHECK(call_strnlen(terminated, 2) == 2);
    CHECK(call_strnlen(unterminated, sizeof(unterminated)) ==
          sizeof(unterminated));
}

static void test_strscpy_sizes_truncation_and_canaries(void)
{
    char size_zero[] = {(char)0xc1, 'X', (char)0xc2};
    const unsigned char size_zero_expected[] = {0xc1, 'X', 0xc2};
    char size_one[] = {(char)0xc1, 'X', (char)0xc2};
    const unsigned char size_one_expected[] = {0xc1, 0x00, 0xc2};
    char empty_one[] = {(char)0xc1, 'X', (char)0xc2};
    const unsigned char empty_one_expected[] = {0xc1, 0x00, 0xc2};
    char fit[] = {(char)0xc1, 'X', 'X', 'X', 'X', 'X', (char)0xc2};
    const unsigned char fit_expected[] = {0xc1, 'c', 'a', 't', 0x00,
                                           'X', 0xc2};
    char exact[] = {(char)0xc1, 'X', 'X', 'X', 'X', (char)0xc2};
    const unsigned char exact_expected[] = {0xc1, 'c', 'a', 't', 0x00, 0xc2};
    char truncated[] = {(char)0xc1, 'X', 'X', 'X', 'X', (char)0xc2};
    const unsigned char truncated_expected[] = {0xc1, 't', 'i', 'g', 0x00,
                                                 0xc2};
    char huge[] = {(char)0xc1, 'X', (char)0xc2};
    const unsigned char huge_expected[] = {0xc1, 'X', 0xc2};

    CHECK(call_strscpy(size_zero + 1, "abc", 0) == -E2BIG);
    CHECK_BYTES(size_zero, size_zero_expected);

    CHECK(call_strscpy(size_one + 1, "abc", 1) == -E2BIG);
    CHECK_BYTES(size_one, size_one_expected);

    CHECK(call_strscpy(empty_one + 1, "", 1) == 0);
    CHECK_BYTES(empty_one, empty_one_expected);

    CHECK(call_strscpy(fit + 1, "cat", 5) == 3);
    CHECK_BYTES(fit, fit_expected);

    CHECK(call_strscpy(exact + 1, "cat", 4) == 3);
    CHECK_BYTES(exact, exact_expected);

    CHECK(call_strscpy(truncated + 1, "tiger", 4) == -E2BIG);
    CHECK_BYTES(truncated, truncated_expected);

    CHECK(call_strscpy(huge + 1, "a", (size_t)INT_MAX + 1U) == -E2BIG);
    CHECK_BYTES(huge, huge_expected);
}

static void test_strscpy_pad_success_and_failure(void)
{
    char short_src[] = {(char)0xc1, 'X', 'X', 'X', 'X', 'X', (char)0xc2};
    const unsigned char short_expected[] = {0xc1, 'h', 'i', 0x00,
                                             0x00, 0x00, 0xc2};
    char exact[] = {(char)0xc1, 'X', 'X', 'X', 'X', (char)0xc2};
    const unsigned char exact_expected[] = {0xc1, 'c', 'a', 't', 0x00, 0xc2};
    char truncated[] = {(char)0xc1, 'X', 'X', 'X', 'X', (char)0xc2};
    const unsigned char truncated_expected[] = {0xc1, 't', 'i', 'g', 0x00,
                                                 0xc2};
    char zero[] = {(char)0xc1, 'X', (char)0xc2};
    const unsigned char zero_expected[] = {0xc1, 'X', 0xc2};
    char size_one[] = {(char)0xc1, 'X', (char)0xc2};
    const unsigned char size_one_expected[] = {0xc1, 0x00, 0xc2};

    CHECK(call_strscpy_pad(short_src + 1, "hi", 5) == 2);
    CHECK_BYTES(short_src, short_expected);

    CHECK(call_strscpy_pad(exact + 1, "cat", 4) == 3);
    CHECK_BYTES(exact, exact_expected);

    CHECK(call_strscpy_pad(truncated + 1, "tiger", 4) == -E2BIG);
    CHECK_BYTES(truncated, truncated_expected);

    CHECK(call_strscpy_pad(zero + 1, "abc", 0) == -E2BIG);
    CHECK_BYTES(zero, zero_expected);

    CHECK(call_strscpy_pad(size_one + 1, "abc", 1) == -E2BIG);
    CHECK_BYTES(size_one, size_one_expected);
}

static void test_memcpy_binary_zero_length_and_canaries(void)
{
    const unsigned char source[] = {0x11, 0x00, 0x7f, 0x80, 0xff};
    unsigned char dest[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char expected[] = {0xc1, 0x11, 0x00, 0x7f,
                                      0x80, 0xff, 0xc2};
    unsigned char zero[] = {0xc1, 0xaa, 0xc2};
    const unsigned char zero_expected[] = {0xc1, 0xaa, 0xc2};
    void *ret;

    ret = call_memcpy(dest + 1, source, sizeof(source));
    CHECK(ret == dest + 1);
    CHECK_BYTES(dest, expected);

    ret = call_memcpy(zero + 1, source, 0);
    CHECK(ret == zero + 1);
    CHECK_BYTES(zero, zero_expected);
}

static void test_memmove_overlap_directions(void)
{
    unsigned char move_right[] = {0xc1, 'a', 'b', 'c', 'd', 'e', 0xc2};
    const unsigned char right_expected[] = {0xc1, 'a', 'a', 'b',
                                             'c', 'd', 0xc2};
    unsigned char move_left[] = {0xc1, 'a', 'b', 'c', 'd', 'e', 0xc2};
    const unsigned char left_expected[] = {0xc1, 'b', 'c', 'd',
                                            'e', 'e', 0xc2};
    unsigned char adjacent[] = {0xc1, 'a', 'b', 'c', 'd', 'e', 'f', 0xc2};
    const unsigned char adjacent_expected[] = {0xc1, 'a', 'b', 'c',
                                                'a', 'b', 'c', 0xc2};
    unsigned char same[] = {0xc1, 'a', 'b', 'c', 0xc2};
    const unsigned char same_expected[] = {0xc1, 'a', 'b', 'c', 0xc2};
    void *ret;

    ret = call_memmove(move_right + 2, move_right + 1, 4);
    CHECK(ret == move_right + 2);
    CHECK_BYTES(move_right, right_expected);

    ret = call_memmove(move_left + 1, move_left + 2, 4);
    CHECK(ret == move_left + 1);
    CHECK_BYTES(move_left, left_expected);

    ret = call_memmove(adjacent + 4, adjacent + 1, 3);
    CHECK(ret == adjacent + 4);
    CHECK_BYTES(adjacent, adjacent_expected);

    ret = call_memmove(same + 1, same + 1, 3);
    CHECK(ret == same + 1);
    CHECK_BYTES(same, same_expected);

    ret = call_memmove(same + 2, same + 1, 0);
    CHECK(ret == same + 2);
    CHECK_BYTES(same, same_expected);
}

static void test_memcpy_pad_copy_truncate_and_pad(void)
{
    const unsigned char source[] = {0x11, 0x00, 0x33, 0x44, 0x55};
    unsigned char padded[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char padded_expected[] = {0xc1, 0x11, 0x00, 0xfe,
                                              0xfe, 0xfe, 0xc2};
    unsigned char truncated[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char truncated_expected[] = {0xc1, 0x11, 0x00, 0x33, 0xc2};
    unsigned char all_pad[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char all_pad_expected[] = {0xc1, 0x7e, 0x7e, 0x7e, 0xc2};
    unsigned char exact[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char exact_expected[] = {0xc1, 0x11, 0x00, 0x33, 0xc2};
    unsigned char zero[] = {0xc1, 0xaa, 0xc2};
    const unsigned char zero_expected[] = {0xc1, 0xaa, 0xc2};
    void *ret;

    ret = call_memcpy_pad(padded + 1, 5, source, 2, 0x1fe);
    CHECK(ret == padded + 1);
    CHECK_BYTES(padded, padded_expected);

    ret = call_memcpy_pad(truncated + 1, 3, source, sizeof(source), 0x7e);
    CHECK(ret == truncated + 1);
    CHECK_BYTES(truncated, truncated_expected);

    ret = call_memcpy_pad(all_pad + 1, 3, source, 0, 0x7e);
    CHECK(ret == all_pad + 1);
    CHECK_BYTES(all_pad, all_pad_expected);

    ret = call_memcpy_pad(exact + 1, 3, source, 3, 0x7e);
    CHECK(ret == exact + 1);
    CHECK_BYTES(exact, exact_expected);

    ret = call_memcpy_pad(zero + 1, 0, source, sizeof(source), 0x7e);
    CHECK(ret == zero + 1);
    CHECK_BYTES(zero, zero_expected);
}

static void test_strtomem_no_terminator_and_bounds(void)
{
    unsigned char short_src[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char short_expected[] = {0xc1, 'h', 'i', 0xaa, 0xaa, 0xc2};
    const char unterminated[] = {'a', 'b', 'c', 'd', 'e'};
    unsigned char full[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char full_expected[] = {0xc1, 'a', 'b', 'c', 0xc2};
    unsigned char empty[] = {0xc1, 0xaa, 0xaa, 0xc2};
    const unsigned char empty_expected[] = {0xc1, 0xaa, 0xaa, 0xc2};
    unsigned char zero[] = {0xc1, 0xaa, 0xc2};
    const unsigned char zero_expected[] = {0xc1, 0xaa, 0xc2};

    call_strtomem(short_src + 1, 4, "hi");
    CHECK_BYTES(short_src, short_expected);

    call_strtomem(full + 1, 3, unterminated);
    CHECK_BYTES(full, full_expected);

    call_strtomem(empty + 1, 2, "");
    CHECK_BYTES(empty, empty_expected);

    call_strtomem(zero + 1, 0, "abc");
    CHECK_BYTES(zero, zero_expected);
}

static void test_strtomem_pad_content_and_bounds(void)
{
    unsigned char short_src[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char short_expected[] = {0xc1, 'h', 'i', 0x7e, 0x7e, 0xc2};
    const char unterminated[] = {'a', 'b', 'c', 'd', 'e'};
    unsigned char full[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char full_expected[] = {0xc1, 'a', 'b', 'c', 0xc2};
    unsigned char empty[] = {0xc1, 0xaa, 0xaa, 0xaa, 0xc2};
    const unsigned char empty_expected[] = {0xc1, 0xfe, 0xfe, 0xfe, 0xc2};
    unsigned char zero[] = {0xc1, 0xaa, 0xc2};
    const unsigned char zero_expected[] = {0xc1, 0xaa, 0xc2};

    call_strtomem_pad(short_src + 1, 4, "hi", 0x7e);
    CHECK_BYTES(short_src, short_expected);

    call_strtomem_pad(full + 1, 3, unterminated, 0x7e);
    CHECK_BYTES(full, full_expected);

    call_strtomem_pad(empty + 1, 3, "", 0x1fe);
    CHECK_BYTES(empty, empty_expected);

    call_strtomem_pad(zero + 1, 0, "abc", 0x7e);
    CHECK_BYTES(zero, zero_expected);
}

static void test_array_sized_wrappers(void)
{
    unsigned char copied[] = {0xaa, 0xaa, 0xaa, 0xaa};
    const unsigned char copied_expected[] = {'h', 'i', 0xaa, 0xaa};
    unsigned char padded[] = {0xaa, 0xaa, 0xaa, 0xaa};
    const unsigned char padded_expected[] = {'h', 'i', 0x7e, 0x7e};

    strtomem_auto(copied, "hi");
    CHECK_BYTES(copied, copied_expected);

    strtomem_pad_auto(padded, "hi", 0x7e);
    CHECK_BYTES(padded, padded_expected);
}

int main(void)
{
    test_memset_edges_and_canaries();
    test_strnlen_bounds();
    test_strscpy_sizes_truncation_and_canaries();
    test_strscpy_pad_success_and_failure();
    test_memcpy_binary_zero_length_and_canaries();
    test_memmove_overlap_directions();
    test_memcpy_pad_copy_truncate_and_pad();
    test_strtomem_no_terminator_and_bounds();
    test_strtomem_pad_content_and_bounds();
    test_array_sized_wrappers();

    if (failures != 0U) {
        (void)fprintf(stderr, "%u of %u checks failed\n", failures, checks);
        return 1;
    }

    (void)printf("PASS: %u checks\n", checks);
    return 0;
}
