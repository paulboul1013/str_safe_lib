# str_safe_lib

## first think
The C string library, `<string.h>`, contains some functions that can be unsafe if used incorrectly, such as `strcpy`, `strncpy`, `strcat`, and the old `gets` function.

I read some Linux news saying that the Linux kernel developers removed uses of `strncpy` from the kernel source code. This does not mean that `strncpy` was removed from the user-space C library. You can still use `<string.h>` in normal C programs, but the Linux kernel is moving away from unsafe string functions.

This repository is for learning how to write simple and safer versions of common `<string.h>` functions.


## string function list
- strscpy
- strscpy_pad

## warning
This repository does not implement compile-time type checks or macro-based safety features. It only provides simple implementations for learning purposes.
