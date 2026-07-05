#include "String.h"


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
        memset(dst+ret+1,'\0',size-ret-1);
    }

    return ret;
}