#include <stdio.h>

int main(void) {
    int hp = 1000;
    unsigned char *p = (unsigned char *)&hp;

    printf("变量 hp 的地址：%p\n", (void *)&hp);
    printf("它占 %zu 字节，每个字节是：\n", sizeof(hp));
    for (int i = 0; i < sizeof(hp); i++) {
        printf("  [%d] 0x%02X  (%u)\n", i, p[i], p[i]);
    }
    return 0;
}