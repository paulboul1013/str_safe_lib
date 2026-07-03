#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

// #define E2BIG 7

//compile-time check 
#define __same_type(a,b) \
    __builtin_types_compatible_p(__typeof__(a),__typeof__(b))

ssize_t strscpy(char *dst, const char *src,size_t size);
ssize_t strscpy_pad(char *dst, const char *src,size_t size);

// strscpy two parameters version
#define  strscpy_auto(dst,src) \
    strscpy((dst),(src),sizeof(dst))

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
    
    char a='a';
    int int_a=96;
    char b='b';
    printf("%d\n",__same_type(int_a,b));


    return 0;
}