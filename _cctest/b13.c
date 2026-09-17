#include <stdio.h>
#include <stdint.h>

void hexdump(const void *addr, size_t len) {
    const uint8_t *p = (const uint8_t *)addr;
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) printf("\n%04zX  ", i);
        printf("%02X ", p[i]);
    }
    printf("\n");
}

int main(void) {
    int hp = 1000;
    hexdump(&hp, sizeof(hp));
    return 0;
}