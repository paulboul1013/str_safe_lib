#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>


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

ssize_t strscpy(char *dst, const char *src,size_t size);
ssize_t strscpy_pad(char *dst, const char *src,size_t size);

// strscpy two parameters version
#define  strscpy_auto(dst,src) \
    strscpy((dst),(src),sizeof(dst))


void *memset(void *s,int c,size_t count) {
    char *ss=s;

    while (count--){
        *ss++=c;
    }

    return s;
}

size_t strnlen(const char *s, size_t maxlen){
    const char *src;
    
    for(src=s; maxlen-- && *src!='\0';++src);

    return src-s;
}
ssize_t strscpy(char *dst, const char *src,size_t size) {
    size_t i;

    if (size == 0 || size > INT_MAX) {
        return  -E2BIG;
    }

    for(i=0;i<size-1;i++){
        dst[i]=src[i];

        if (src[i]=='\0') {
            return (ssize_t)i;
        }
    }

    dst[i] = '\0';
    
    return src[i] ? -E2BIG : (ssize_t)i;
}

ssize_t strscpy_pad(char *dst, const char *src,size_t size){
    ssize_t ret;
    
    ret = strscpy(dst,src,size);

    if (ret>=0 && ret<size){ //fill zero elements
        //dst[ret] is '\0'
        //from dst[ret+1] begin fill zeros
        memset(dst+ret+1,0,size-ret-1);
    }

    return ret;
}

int main() {
    
    char *a=(char*)malloc(sizeof(char)*10);

    memset(a,'z',sizeof(char)*10);
    for(int i=0;i<10;i++){
        printf("%c ",a[i]);
    }

    printf("\n");

    return 0;
}