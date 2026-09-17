#include <stdio.h>

int main(void) {
    int hp = 100;
    int *p = &hp;      // p 指向 hp

    printf("hp = %d\n", hp);        // 100
    printf("*p = %d\n", *p);        // 100（和 hp 一样）
    printf("p = %p\n", (void *)p);  // 打印 p 里存的地址
    printf("&hp = %p\n", (void *)&hp);  // 打印 hp 的地址（和 p 一样）

    *p = 200;          // 通过指针改 hp
    printf("hp = %d\n", hp);        // 200（被改了）

    hp = 300;          // 直接改 hp
    printf("*p = %d\n", *p);        // 300（通过指针看到的也变了）

    return 0;
}