#define _POSIX_C_SOURCE 200809L

#include "../String.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MIB (1024U * 1024U)
#define TARGET_BYTES (32U * MIB)
#define TRIALS 3

typedef void *(*memset_fn)(void *, int, size_t);
typedef size_t (*strnlen_fn)(const char *, size_t);
typedef void *(*memcpy_fn)(void *, const void *, size_t);
typedef void *(*memmove_fn)(void *, const void *, size_t);
typedef void (*bench_action)(void *);

/* Force calls to the repo implementation even when the compiler has builtins. */
static memset_fn volatile project_memset = memset;
static strnlen_fn volatile project_strnlen = strnlen;
static memcpy_fn volatile project_memcpy = memcpy;
static memmove_fn volatile project_memmove = memmove;
static ssize_t (*volatile project_strscpy)(char *, const char *, size_t) =
    strscpy;
static ssize_t (*volatile project_strscpy_pad)(char *, const char *, size_t) =
    strscpy_pad;
static void *(*volatile project_memcpy_pad)(void *, size_t, const void *,
                                            size_t, int) = memcpy_pad;
static void (*volatile project_strtomem)(void *, size_t, const char *) =
    strtomem;
static void (*volatile project_strtomem_pad)(void *, size_t, const char *, int) =
    strtomem_pad;

static volatile uint64_t result_sink;

struct bench_context {
    unsigned char *dst;
    unsigned char *src;
    size_t size;
    memset_fn libc_memset;
    strnlen_fn libc_strnlen;
    memcpy_fn libc_memcpy;
    memmove_fn libc_memmove;
};

static uint64_t nanoseconds_now(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        perror("clock_gettime");
        exit(EXIT_FAILURE);
    }
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) + (uint64_t)ts.tv_nsec;
}

static void ref_copy(void *dest, const void *src, size_t count)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    size_t i;

    for (i = 0; i < count; ++i)
        d[i] = s[i];
}

static size_t ref_bounded_length(const char *src, size_t maxlen)
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
    size_t i;

    if (result >= 0) {
        for (i = (size_t)result + 1; i < size; ++i)
            dst[i] = '\0';
    }
    return result;
}

static void ref_memcpy_pad(void *dest, size_t dest_len, const void *src,
                           size_t src_len, int pad)
{
    unsigned char *d = dest;
    size_t copied = src_len < dest_len ? src_len : dest_len;
    size_t i;

    ref_copy(d, src, copied);
    for (i = copied; i < dest_len; ++i)
        d[i] = (unsigned char)pad;
}

static void ref_strtomem(void *dest, size_t dest_len, const char *src)
{
    ref_copy(dest, src, ref_bounded_length(src, dest_len));
}

static void ref_strtomem_pad(void *dest, size_t dest_len, const char *src,
                             int pad)
{
    size_t len = ref_bounded_length(src, dest_len);
    unsigned char *d = dest;
    size_t i;

    ref_copy(d, src, len);
    for (i = len; i < dest_len; ++i)
        d[i] = (unsigned char)pad;
}

static void act_project_memcpy(void *opaque)
{
    struct bench_context *c = opaque;
    project_memcpy(c->dst, c->src, c->size);
}

static void act_libc_memcpy(void *opaque)
{
    struct bench_context *c = opaque;
    c->libc_memcpy(c->dst, c->src, c->size);
}

static void act_project_memmove(void *opaque)
{
    struct bench_context *c = opaque;
    project_memmove(c->dst + 1, c->dst, c->size);
}

static void act_libc_memmove(void *opaque)
{
    struct bench_context *c = opaque;
    c->libc_memmove(c->dst + 1, c->dst, c->size);
}

static void act_project_memset(void *opaque)
{
    struct bench_context *c = opaque;
    project_memset(c->dst, 0x5a, c->size);
}

static void act_libc_memset(void *opaque)
{
    struct bench_context *c = opaque;
    c->libc_memset(c->dst, 0x5a, c->size);
}

static void act_project_strnlen(void *opaque)
{
    struct bench_context *c = opaque;
    result_sink += project_strnlen((const char *)c->src, c->size);
}

static void act_libc_strnlen(void *opaque)
{
    struct bench_context *c = opaque;
    result_sink += c->libc_strnlen((const char *)c->src, c->size);
}

static void act_project_strscpy(void *opaque)
{
    struct bench_context *c = opaque;
    result_sink += (uint64_t)project_strscpy((char *)c->dst,
                                             (const char *)c->src, c->size);
}

static void act_ref_strscpy(void *opaque)
{
    struct bench_context *c = opaque;
    result_sink +=
        (uint64_t)ref_strscpy((char *)c->dst, (const char *)c->src, c->size);
}

static void act_project_strscpy_pad(void *opaque)
{
    struct bench_context *c = opaque;
    result_sink += (uint64_t)project_strscpy_pad(
        (char *)c->dst, (const char *)c->src, c->size);
}

static void act_ref_strscpy_pad(void *opaque)
{
    struct bench_context *c = opaque;
    result_sink += (uint64_t)ref_strscpy_pad(
        (char *)c->dst, (const char *)c->src, c->size);
}

static void act_project_memcpy_pad(void *opaque)
{
    struct bench_context *c = opaque;
    project_memcpy_pad(c->dst, c->size, c->src, c->size / 2, 0xa5);
}

static void act_ref_memcpy_pad(void *opaque)
{
    struct bench_context *c = opaque;
    ref_memcpy_pad(c->dst, c->size, c->src, c->size / 2, 0xa5);
}

static void act_project_strtomem(void *opaque)
{
    struct bench_context *c = opaque;
    project_strtomem(c->dst, c->size, (const char *)c->src);
}

static void act_ref_strtomem(void *opaque)
{
    struct bench_context *c = opaque;
    ref_strtomem(c->dst, c->size, (const char *)c->src);
}

static void act_project_strtomem_pad(void *opaque)
{
    struct bench_context *c = opaque;
    project_strtomem_pad(c->dst, c->size, (const char *)c->src, 0xa5);
}

static void act_ref_strtomem_pad(void *opaque)
{
    struct bench_context *c = opaque;
    ref_strtomem_pad(c->dst, c->size, (const char *)c->src, 0xa5);
}

static double time_action(bench_action action, struct bench_context *context,
                          size_t iterations)
{
    uint64_t begin;
    uint64_t end;
    size_t i;

    action(context);
    begin = nanoseconds_now();
    for (i = 0; i < iterations; ++i)
        action(context);
    end = nanoseconds_now();
    result_sink += context->dst[context->size / 2];
    return (double)(end - begin) / 1000000000.0;
}

static double best_time(bench_action action, struct bench_context *context,
                        size_t iterations)
{
    double best = 1.0e100;
    int trial;

    for (trial = 0; trial < TRIALS; ++trial) {
        double elapsed = time_action(action, context, iterations);

        if (elapsed < best)
            best = elapsed;
    }
    return best;
}

static void report_case(const char *operation, const char *reference_name,
                        size_t size, bench_action project_action,
                        bench_action reference_action,
                        struct bench_context *context)
{
    size_t iterations = TARGET_BYTES / (size == 0 ? 1 : size);
    double project_seconds;
    double reference_seconds;
    double total_mib;
    double project_rate;
    double reference_rate;
    size_t i;

    if (iterations < 64)
        iterations = 64;
    if (iterations > 2000000)
        iterations = 2000000;
    context->size = size;
    /* Make string operations inspect exactly size bytes for every case. */
    for (i = 0; i < size; ++i)
        context->src[i] = (unsigned char)(1U + i % 251U);
    context->src[size - 1U] = '\0';
    project_seconds = best_time(project_action, context, iterations);
    reference_seconds = best_time(reference_action, context, iterations);
    total_mib = (double)size * (double)iterations / (double)MIB;
    project_rate = total_mib / project_seconds;
    reference_rate = total_mib / reference_seconds;

    printf("%-15s %8zu  %12.1f  %12.1f  %8.3fx  %s\n", operation, size,
           project_rate, reference_rate, project_rate / reference_rate,
           reference_name);
}

static void *open_libc(void)
{
    void *handle = dlopen("libc.so.6", RTLD_LAZY | RTLD_LOCAL);

    if (handle == NULL)
        fprintf(stderr, "unable to load libc.so.6: %s\n", dlerror());
    return handle;
}

static void load_libc_functions(void *handle, struct bench_context *context)
{
    union {
        void *object;
        memset_fn function;
    } memset_symbol;
    union {
        void *object;
        strnlen_fn function;
    } strnlen_symbol;
    union {
        void *object;
        memcpy_fn function;
    } memcpy_symbol;
    union {
        void *object;
        memmove_fn function;
    } memmove_symbol;

    memset_symbol.object = dlsym(handle, "memset");
    strnlen_symbol.object = dlsym(handle, "strnlen");
    memcpy_symbol.object = dlsym(handle, "memcpy");
    memmove_symbol.object = dlsym(handle, "memmove");
    context->libc_memset = memset_symbol.function;
    context->libc_strnlen = strnlen_symbol.function;
    context->libc_memcpy = memcpy_symbol.function;
    context->libc_memmove = memmove_symbol.function;
    if (context->libc_memset == NULL || context->libc_strnlen == NULL ||
        context->libc_memcpy == NULL || context->libc_memmove == NULL) {
        fprintf(stderr, "unable to resolve required libc symbols: %s\n",
                dlerror());
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    static const size_t sizes[] = {32, 256, 4096, 65536};
    const size_t allocation_size = sizes[sizeof(sizes) / sizeof(sizes[0]) - 1];
    struct bench_context context;
    void *libc_handle;
    size_t i;

    context.dst = malloc(allocation_size + 1);
    context.src = malloc(allocation_size + 1);
    if (context.dst == NULL || context.src == NULL) {
        fprintf(stderr, "unable to allocate benchmark buffers\n");
        free(context.dst);
        free(context.src);
        return EXIT_FAILURE;
    }
    for (i = 0; i <= allocation_size; ++i) {
        context.src[i] = (unsigned char)(1U + i % 251U);
        context.dst[i] = 0;
    }
    libc_handle = open_libc();
    if (libc_handle == NULL) {
        free(context.dst);
        free(context.src);
        return EXIT_FAILURE;
    }
    load_libc_functions(libc_handle, &context);

    printf("warm-cache throughput, best of %d trials; ratio = project/reference\n",
           TRIALS);
    printf("operation          bytes  project MiB/s   ref MiB/s     ratio  reference\n");
    for (i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        report_case("memcpy", "libc", sizes[i], act_project_memcpy,
                    act_libc_memcpy, &context);
        report_case("memmove+1", "libc", sizes[i], act_project_memmove,
                    act_libc_memmove, &context);
        report_case("memset", "libc", sizes[i], act_project_memset,
                    act_libc_memset, &context);
        report_case("strnlen", "libc", sizes[i], act_project_strnlen,
                    act_libc_strnlen, &context);
    }

    /* The repo-specific APIs are compared with small, transparent C models. */
    report_case("strscpy", "C model", 256, act_project_strscpy,
                act_ref_strscpy, &context);
    report_case("strscpy_pad", "C model", 256, act_project_strscpy_pad,
                act_ref_strscpy_pad, &context);
    report_case("memcpy_pad", "C model", 256, act_project_memcpy_pad,
                act_ref_memcpy_pad, &context);
    report_case("strtomem", "C model", 256, act_project_strtomem,
                act_ref_strtomem, &context);
    report_case("strtomem_pad", "C model", 256, act_project_strtomem_pad,
                act_ref_strtomem_pad, &context);

    printf("sink=%" PRIu64 " (prevents dead-code elimination)\n", result_sink);
    dlclose(libc_handle);
    free(context.dst);
    free(context.src);
    return EXIT_SUCCESS;
}
