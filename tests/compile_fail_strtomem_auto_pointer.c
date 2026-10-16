#include "../String.h"

void unsafe_pointer_capacity(void *destination, const char *source);

void unsafe_pointer_capacity(void *destination, const char *source)
{
    strtomem_auto(destination, source);
}
