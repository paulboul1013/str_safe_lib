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

void *memcpy(void *dest,const void *src,size_t n) {
    char *temp=dest;
    const char *s=src;

    while(n--) {
        *temp++=*s++;
    }
    return dest;
}

void *memmove(void *dest,const void *src,size_t n) {
    uint8_t *d = dest;
	const uint8_t *s = src;

    if (s > d  || (s+n)<=d ){ //check memory overlap 
        // for(size_t i=0;i<n;i++){ //forward copy
        //     dst[i]=src[i];
        // }
        //no memory overlap
        return memcpy(dest,src,n);
    }
    else{
        for(ssize_t i = n-1; i>=0; i--){ //reverse copy
            d[i]=s[i];
        }
    }

    return dest;
}