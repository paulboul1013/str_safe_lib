#ifndef STR_H
#define STR_H

#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include <stdint.h>

#define E2BIG 7

//compile-time check 
#define __same_type(a,b) \
    __builtin_types_compatible_p(__typeof__(a),__typeof__(b))


#define __BUILD_BUG_ON_ZERO_MSG(e, msg, ...) ((int)sizeof(struct {_Static_assert(!(e), msg);}))

//assert at compile time if e is zero
#define BUILD_BUG_ON_ZERO(e, ...) \
    __BUILD_BUG_ON_ZERO_MSG(e, ##__VA_ARGS__, #e " is true")

#define __must_be_array(a) \
    BUILD_BUG_ON_ZERO(__same_type((a), &(a)[0]))


void *memset(void *s,int c,size_t count);
size_t strnlen(const char *s, size_t maxlen);
ssize_t strscpy(char *dst, const char *src,size_t size); //string copy for size len
ssize_t strscpy_pad(char *dst, const char *src,size_t size); //string copy for size len and left fill '\0' 
void *memcpy(void *dest,const void *src,size_t n); //memory must be not overlap
void *memmove(void *dest,const void *src,size_t n); //same as memcpy but have memory overlap
void *memcpy_pad(void *des,size_t dest_len,const void *src,size_t src_len,int pad);

// strscpy two parameters version
#define  strscpy_auto(dst,src) \
    strscpy((dst),(src),sizeof(dst))




#endif