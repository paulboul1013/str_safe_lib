#include <stdio.h>
#include <stdlib.h>
// #include <errno.h>

#include "String.h"


int main(void) {
    


    int src_array[5] = {1, 2, 3, 4, 5};
    int dst_array[5];
    memcpy_pad(dst_array,sizeof(dst_array),src_array,sizeof(src_array)/sizeof(src_array[0])*2,0);
    for(int i=0;i<5;i++){
        printf("%d ",dst_array[i]);
    }
    printf("\n");

    return 0;
}
