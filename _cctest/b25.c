#include <stdio.h>
#include <stdint.h>

struct A {
    char  a;   // 1 字节
    int   b;   // 4 字节
    char  c;   // 1 字节
};

int main(void) {
    printf("sizeof(A) = %zu\n", sizeof(struct A));
    return 0;
}