#include <stdio.h>
#include <stdlib.h>
// #include <errno.h>

#include "String.h"


int main() {
    
    char s[6];
    memmove(s,"hello",6);
    printf("%s\n",s);

    char s1[6];
    memcpy(s1,"world",6);
    printf("%s\n",s1);

    return 0;
}