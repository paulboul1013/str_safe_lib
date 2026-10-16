#include "../String.h"

ssize_t unsafe_pointer_capacity(char *destination, const char *source);

ssize_t unsafe_pointer_capacity(char *destination, const char *source)
{
    return strscpy_auto(destination, source);
}
