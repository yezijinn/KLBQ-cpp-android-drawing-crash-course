#include <stdio.h>

int main(void) {
    int x = 10;
    int y = 20;
    int *p = &x;      // p 指向 x

    printf("x=%d, *p=%d\n", x, *p);      // 10, 10

    *p = 99;                              // 通过 p 改 x
    printf("x=%d, *p=%d\n", x, *p);      // 99, 99

    p = &y;                               // 让 p 改指向 y（指针可以改指向）
    printf("y=%d, *p=%d\n", y, *p);      // 20, 20

    *p = 88;                              // 通过 p 改 y
    printf("x=%d, y=%d\n", x, y);        // 99, 88

    return 0;
}