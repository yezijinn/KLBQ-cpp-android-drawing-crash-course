#include <stdio.h>

int add(int a, int b) {
    return a + b;
}

int main(void) {
    int result = add(3, 4);    // 调用：把 3 和 4 传进去，返回值存进 result
    printf("%d\n", result);    // 输出 7

    printf("%d\n", add(10, 20));  // 调用结果直接当参数，输出 30
    return 0;
}