#include <stdio.h>
#include <stdint.h>          // uint16_t / uint8_t

int is_little_endian(void) {
    uint16_t x = 0x0001;
    return *(uint8_t*)&x == 0x01;      // 低地址是 01 → 小端
}

int main(void) {
    printf("%s\n", is_little_endian() ? "小端" : "大端");
    return 0;
}