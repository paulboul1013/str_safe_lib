#include <stdio.h>
#include <stdlib.h>
// #include <errno.h>

#include "String.h"


int main() {
    
    char test[10];
    char *test2="hello";
    strscpy_auto(test,test2);
    printf("%s\n",test);

    char test3[10];
    char *pad_left="aaaaa";
    strscpy_pad(test3,pad_left,6);
    printf("%s\n",test3);

    return 0;
}