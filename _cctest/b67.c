// demo.c
#include <stdio.h>

int global_init = 42;        // .data
int global_zero;             // .bss
const char *msg = "hi";      // .rodata

int add(int a, int b) { return a + b; }     // 已定义

int main(void) {
    printf("%d\n", add(1, 2));
    return 0;
}