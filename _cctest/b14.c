#include <stdio.h>

int main(void) {
    int hp = 1000;
    printf("hp 的值是：%d\n", hp);
    printf("hp 的地址是：%p\n", (void *)&hp);
    return 0;
}