#ifdef STRING_HEADER_FIRST
#include "../String.h"
#include <errno.h>
#include <string.h>
#else
#include <errno.h>
#include <string.h>
#include "../String.h"
#endif

void *header_compatibility_probe(void *destination, const void *source,
                                 size_t count);

void *header_compatibility_probe(void *destination, const void *source,
                                 size_t count)
{
    return memcpy(destination, source, count);
}
