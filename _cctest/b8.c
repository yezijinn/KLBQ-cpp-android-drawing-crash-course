#include <stdio.h>

int main(void) {
    int hp = 100;
    printf("初始 hp = %d\n", hp);

    hp = 80;
    printf("改成 80 后 hp = %d\n", hp);

    hp = hp - 20;   // 右边先算 hp - 20 = 60，再放回 hp
    printf("再减 20 后 hp = %d\n", hp);

    return 0;
}