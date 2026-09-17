#include <stdio.h>

int main(void) {
    int a = 5;
    int b = 3;

    printf("a + b = %d\n", a + b);      // 加法
    printf("a - b = %d\n", a - b);      // 减法
    printf("a * b = %d\n", a * b);      // 乘法
    printf("a / b = %d\n", a / b);      // 整数除法（注意结果！）
    printf("a %% b = %d\n", a % b);     // 取余
    return 0;
}