#include "../String.h"

void unsafe_pointer_capacity(void *destination, const char *source);

void unsafe_pointer_capacity(void *destination, const char *source)
{
    strtomem_pad_auto(destination, source, 0);
}
