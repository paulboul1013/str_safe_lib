#define _GNU_SOURCE

#include "../String.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

struct guarded_page {
    unsigned char *mapping;
    unsigned char *data;
    size_t page_size;
};

static struct guarded_page guarded_page_create(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    struct guarded_page page = {0};

    if (page_size <= 0) {
        perror("sysconf");
        exit(EXIT_FAILURE);
    }

    page.page_size = (size_t)page_size;
    page.mapping = mmap(NULL, page.page_size * 3U, PROT_NONE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (page.mapping == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    page.data = page.mapping + page.page_size;
    if (mprotect(page.data, page.page_size, PROT_READ | PROT_WRITE) != 0) {
        perror("mprotect");
        exit(EXIT_FAILURE);
    }

    return page;
}

static void guarded_page_destroy(struct guarded_page *page)
{
    if (munmap(page->mapping, page->page_size * 3U) != 0) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }
}

static void require(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    struct guarded_page source_page = guarded_page_create();
    struct guarded_page destination_page = guarded_page_create();
    const size_t width = 32U;
    char *source = (char *)(source_page.data + source_page.page_size - width);
    char *destination =
        (char *)(destination_page.data + destination_page.page_size - width);
    size_t index;

    for (index = 0U; index < width; ++index) {
        source[index] = 'x';
        destination[index] = (char)0x5a;
    }

    require(strnlen(source, width) == width,
            "strnlen read beyond maxlen at a guard page");
    require(strscpy(destination, source, width) == -(ssize_t)E2BIG,
            "strscpy did not report truncation at a guard page");
    require(destination[width - 1U] == '\0',
            "strscpy did not terminate a boundary-sized destination");

    source[width - 1U] = '\0';
    require(strscpy(destination, source, width) == (ssize_t)(width - 1U),
            "strscpy failed for a source ending at a guard page");

    source[7] = '\0';
    for (index = 0U; index < width; ++index) {
        destination[index] = (char)0xa5;
    }
    require(strscpy_pad(destination, source, width) == 7,
            "strscpy_pad returned the wrong length");
    for (index = 8U; index < width; ++index) {
        require(destination[index] == '\0',
                "strscpy_pad did not clear the entire destination tail");
    }

    /* Zero-length operations must not touch an inaccessible page. */
    require(strnlen((const char *)source_page.mapping, 0U) == 0U,
            "strnlen touched memory when maxlen was zero");
    require(strscpy((char *)destination_page.mapping,
                    (const char *)source_page.mapping, 0U) == -(ssize_t)E2BIG,
            "strscpy touched memory when size was zero");
    require(memset(destination_page.mapping, 0, 0U) == destination_page.mapping,
            "memset returned the wrong pointer for a zero-length operation");
    require(memcpy(destination_page.mapping, source_page.mapping, 0U) ==
                destination_page.mapping,
            "memcpy returned the wrong pointer for a zero-length operation");
    require(memmove(destination_page.mapping, source_page.mapping, 0U) ==
                destination_page.mapping,
            "memmove returned the wrong pointer for a zero-length operation");

    guarded_page_destroy(&destination_page);
    guarded_page_destroy(&source_page);
    puts("guard-page tests passed");
    return EXIT_SUCCESS;
}
