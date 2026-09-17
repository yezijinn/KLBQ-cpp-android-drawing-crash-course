#include <stdio.h>
#include <stdint.h>
#include <assert.h>

// 布局： [31:24] type | [23:16] flags | [15:8] index | [7:0] count
uint32_t pack(uint8_t type, uint8_t flags, uint8_t index, uint8_t count) {
    return ((uint32_t)type  << 24) |
           ((uint32_t)flags << 16) |
           ((uint32_t)index << 8)  |
           ((uint32_t)count);
}

void unpack(uint32_t v, uint8_t *type, uint8_t *flags,
            uint8_t *index, uint8_t *count) {
    *type  = (v >> 24) & 0xFF;
    *flags = (v >> 16) & 0xFF;
    *index = (v >> 8)  & 0xFF;
    *count = v & 0xFF;
}

int main(void) {
    uint32_t v = pack(0xAB, 0xCD, 0xEF, 0x12);
    printf("packed = 0x%08X\n", v);        // 期望 0xABCDEF12

    uint8_t t, f, i, c;
    unpack(v, &t, &f, &i, &c);
    assert(t == 0xAB && f == 0xCD && i == 0xEF && c == 0x12);
    printf("unpack OK\n");
    return 0;
}