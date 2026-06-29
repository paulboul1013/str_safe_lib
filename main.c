#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

// #define E2BIG 7

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
    
    char dst[8];
    ssize_t ret = strscpy(dst,"abcdefg",sizeof(dst));
    if (ret==-E2BIG){
        fprintf(stderr,"failed str copy\n");
        return -1;
    }

    printf("%s\n",dst);
    printf("%ld\n",ret);

    char dst2[4];
    ssize_t ret2=strscpy_auto(dst2,"abc");
    printf("%s\n",dst2);
    printf("%ld\n",ret2);

    char dst3[10];
    ssize_t ret3=strscpy_pad(dst3,"xyz",sizeof(dst3));
    int count=0;
    
    for(int i=0;i<sizeof(dst3);i++){
        if (dst3[i]=='\0'){
            count++;
        }
        printf("%c",dst3[i]);
    }
    printf("count \'\\0\':%d\n",count);
    printf("\n");


    return 0;
}