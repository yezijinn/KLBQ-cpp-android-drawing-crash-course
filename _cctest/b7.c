// main.c
#include <stdio.h>
int add(int a, int b);          // 声明：告诉编译器有这个函数

int main(void) {
    printf("%d\n", add(3, 4));
    return 0;
}