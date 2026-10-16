#include "../String.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define ARENA_SIZE 521U
#define DEFAULT_ITERATIONS 100000U
#define FIXED_SEED UINT64_C(0x6a09e667f3bcc909)

/*
 * Volatile function pointers are intentional.  They prevent compilers from
 * replacing calls to this project's libc-named functions with builtins.
 */
static void *(*volatile tested_memset)(void *, int, size_t) = memset;
static size_t (*volatile tested_strnlen)(const char *, size_t) = strnlen;
static ssize_t (*volatile tested_strscpy)(char *, const char *, size_t) = strscpy;
static ssize_t (*volatile tested_strscpy_pad)(char *, const char *, size_t) =
    strscpy_pad;
static void *(*volatile tested_memcpy)(void *, const void *, size_t) = memcpy;
static void *(*volatile tested_memmove)(void *, const void *, size_t) = memmove;
static void *(*volatile tested_memcpy_pad)(void *, size_t, const void *, size_t,
                                           int) = memcpy_pad;
static void (*volatile tested_strtomem)(void *, size_t, const char *) = strtomem;
static void (*volatile tested_strtomem_pad)(void *, size_t, const char *, int) =
    strtomem_pad;

static uint64_t random_state = FIXED_SEED;

static uint64_t random_u64(void)
{
    uint64_t x = random_state;

    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    random_state = x;
    return x * UINT64_C(2685821657736338717);
}

static size_t random_below(size_t limit)
{
    return limit == 0 ? 0 : (size_t)(random_u64() % limit);
}

static void ref_fill(unsigned char *dst, unsigned char value, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i)
        dst[i] = value;
}

static void ref_copy(unsigned char *dst, const unsigned char *src, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i)
        dst[i] = src[i];
}

static int bytes_equal(const unsigned char *a, const unsigned char *b,
                       size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i) {
        if (a[i] != b[i])
            return 0;
    }
    return 1;
}

static size_t ref_strnlen(const char *src, size_t maxlen)
{
    size_t i;

    for (i = 0; i < maxlen && src[i] != '\0'; ++i)
        ;
    return i;
}

static ssize_t ref_strscpy(char *dst, const char *src, size_t size)
{
    size_t i;

    if (size == 0 || size > INT_MAX)
        return -E2BIG;

    for (i = 0; i + 1 < size; ++i) {
        dst[i] = src[i];
        if (src[i] == '\0')
            return (ssize_t)i;
    }
    dst[i] = '\0';
    return src[i] == '\0' ? (ssize_t)i : -E2BIG;
}

static ssize_t ref_strscpy_pad(char *dst, const char *src, size_t size)
{
    ssize_t result = ref_strscpy(dst, src, size);

    if (result >= 0) {
        size_t i;

        for (i = (size_t)result + 1; i < size; ++i)
            dst[i] = '\0';
    }
    return result;
}

static void make_random_bytes(unsigned char *dst, size_t count)
{
    size_t i;

    for (i = 0; i < count; ++i)
        dst[i] = (unsigned char)random_u64();
}

static void fail(size_t iteration, const char *property)
{
    fprintf(stderr,
            "FAIL: %s at iteration %zu, seed=0x%016" PRIx64 "\n",
            property, iteration, FIXED_SEED);
    exit(EXIT_FAILURE);
}

static void check_memset(size_t iteration)
{
    unsigned char actual[ARENA_SIZE + 2];
    unsigned char expected[ARENA_SIZE + 2];
    size_t count = random_below(ARENA_SIZE + 1);
    int value = (int)(uint32_t)random_u64();

    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    ref_fill(expected + 1, (unsigned char)value, count);
    if (tested_memset(actual + 1, value, count) != actual + 1 ||
        !bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "memset matches byte-fill model and returns dest");
}

static void check_strnlen(size_t iteration)
{
    char src[ARENA_SIZE + 1];
    size_t i;
    size_t maxlen = random_below(ARENA_SIZE + 1);

    for (i = 0; i < ARRAY_LEN(src); ++i)
        src[i] = (char)(1U + random_below(255));
    if ((random_u64() & 1U) != 0)
        src[random_below(ARENA_SIZE + 1)] = '\0';

    if (tested_strnlen(src, maxlen) != ref_strnlen(src, maxlen))
        fail(iteration, "strnlen matches bounded scan model");
}

static void check_strscpy_family(size_t iteration)
{
    char src[ARENA_SIZE + 1];
    unsigned char actual[ARENA_SIZE + 2];
    unsigned char expected[ARENA_SIZE + 2];
    size_t src_len = random_below(ARENA_SIZE + 1);
    size_t size = random_below(ARENA_SIZE + 1);
    size_t i;
    ssize_t got;
    ssize_t want;

    for (i = 0; i < src_len; ++i)
        src[i] = (char)(1U + random_below(255));
    src[src_len] = '\0';

    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    want = ref_strscpy((char *)expected + 1, src, size);
    got = tested_strscpy((char *)actual + 1, src, size);
    if (got != want || !bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "strscpy return value, termination, and bounds");

    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    want = ref_strscpy_pad((char *)expected + 1, src, size);
    got = tested_strscpy_pad((char *)actual + 1, src, size);
    if (got != want || !bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "strscpy_pad return value, padding, and bounds");

    if (tested_strscpy((char *)actual, src, 0) != -E2BIG ||
        tested_strscpy((char *)actual, src, (size_t)INT_MAX + 1U) != -E2BIG)
        fail(iteration, "strscpy rejects zero and unrepresentable sizes");
}

static void check_memcpy(size_t iteration)
{
    unsigned char src[ARENA_SIZE];
    unsigned char actual[ARENA_SIZE + 2];
    unsigned char expected[ARENA_SIZE + 2];
    size_t count = random_below(ARENA_SIZE + 1);

    make_random_bytes(src, ARRAY_LEN(src));
    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    ref_copy(expected + 1, src, count);
    if (tested_memcpy(actual + 1, src, count) != actual + 1 ||
        !bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "memcpy matches non-overlapping copy model");
}

static void check_memmove(size_t iteration)
{
    unsigned char actual[ARENA_SIZE];
    unsigned char expected[ARENA_SIZE];
    unsigned char snapshot[ARENA_SIZE];
    size_t src_offset = random_below(ARENA_SIZE);
    size_t dst_offset = random_below(ARENA_SIZE);
    size_t src_room = ARENA_SIZE - src_offset;
    size_t dst_room = ARENA_SIZE - dst_offset;
    size_t max_count = src_room < dst_room ? src_room : dst_room;
    size_t count = random_below(max_count + 1);

    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    ref_copy(snapshot, actual, ARRAY_LEN(actual));
    ref_copy(expected + dst_offset, snapshot + src_offset, count);
    if (tested_memmove(actual + dst_offset, actual + src_offset, count) !=
            actual + dst_offset ||
        !bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "memmove matches snapshot model for every overlap");
}

static void check_memcpy_pad(size_t iteration)
{
    unsigned char src[ARENA_SIZE];
    unsigned char actual[ARENA_SIZE + 2];
    unsigned char expected[ARENA_SIZE + 2];
    size_t dest_len = random_below(ARENA_SIZE + 1);
    size_t src_len = random_below(ARENA_SIZE + 1);
    size_t copy_len = src_len < dest_len ? src_len : dest_len;
    int pad = (int)(uint32_t)random_u64();

    make_random_bytes(src, ARRAY_LEN(src));
    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    ref_copy(expected + 1, src, copy_len);
    ref_fill(expected + 1 + copy_len, (unsigned char)pad,
             dest_len - copy_len);
    if (tested_memcpy_pad(actual + 1, dest_len, src, src_len, pad) !=
            actual + 1 ||
        !bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "memcpy_pad copy, padding, return value, and bounds");
}

static void check_strtomem_family(size_t iteration)
{
    char src[ARENA_SIZE];
    unsigned char actual[ARENA_SIZE + 2];
    unsigned char expected[ARENA_SIZE + 2];
    size_t dest_len = random_below(ARENA_SIZE + 1);
    size_t len;
    size_t i;
    int pad = (int)(uint32_t)random_u64();

    for (i = 0; i < ARRAY_LEN(src); ++i)
        src[i] = (char)(1U + random_below(255));
    if ((random_u64() & 1U) != 0 && dest_len != 0)
        src[random_below(dest_len)] = '\0';
    len = ref_strnlen(src, dest_len);

    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    ref_copy(expected + 1, (const unsigned char *)src, len);
    tested_strtomem(actual + 1, dest_len, src);
    if (!bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "strtomem copies bounded non-NUL bytes only");

    make_random_bytes(actual, ARRAY_LEN(actual));
    ref_copy(expected, actual, ARRAY_LEN(actual));
    ref_copy(expected + 1, (const unsigned char *)src, len);
    ref_fill(expected + 1 + len, (unsigned char)pad, dest_len - len);
    tested_strtomem_pad(actual + 1, dest_len, src, pad);
    if (!bytes_equal(actual, expected, ARRAY_LEN(actual)))
        fail(iteration, "strtomem_pad copies then pads the full destination");
}

static void check_strscpy_auto(void)
{
    char dst[5];
    char expected[5];
    ssize_t got;
    ssize_t want;

    ref_fill((unsigned char *)dst, 0xa5, sizeof(dst));
    ref_fill((unsigned char *)expected, 0xa5, sizeof(expected));
    want = ref_strscpy(expected, "abcdef", sizeof(expected));
    got = strscpy_auto(dst, "abcdef");
    if (got != want ||
        !bytes_equal((const unsigned char *)dst,
                     (const unsigned char *)expected, sizeof(dst)))
        fail(0, "strscpy_auto derives a real array's byte capacity");
}

static size_t parse_iterations(int argc, char **argv)
{
    size_t result = 0;
    const char *p;

    if (argc == 1)
        return DEFAULT_ITERATIONS;
    if (argc != 2 || argv[1][0] == '\0') {
        fprintf(stderr, "usage: %s [positive-iteration-count]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    for (p = argv[1]; *p != '\0'; ++p) {
        size_t digit;

        if (*p < '0' || *p > '9') {
            fprintf(stderr, "invalid iteration count: %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        digit = (size_t)(*p - '0');
        if (result > (SIZE_MAX - digit) / 10U) {
            fprintf(stderr, "iteration count is too large: %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        result = result * 10U + digit;
    }
    if (result == 0) {
        fprintf(stderr, "iteration count must be positive\n");
        exit(EXIT_FAILURE);
    }
    return result;
}

int main(int argc, char **argv)
{
    size_t iterations = parse_iterations(argc, argv);
    size_t i;

    check_strscpy_auto();
    for (i = 0; i < iterations; ++i) {
        check_memset(i);
        check_strnlen(i);
        check_strscpy_family(i);
        check_memcpy(i);
        check_memmove(i);
        check_memcpy_pad(i);
        check_strtomem_family(i);
    }

    printf("PASS: %zu deterministic property iterations, seed=0x%016" PRIx64
           "\n",
           iterations, FIXED_SEED);
    return EXIT_SUCCESS;
}
