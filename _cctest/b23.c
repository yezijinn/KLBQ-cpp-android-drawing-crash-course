#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint64_t data;
    int32_t  count;
    int32_t  max;
} TArray;

int array_valid(const TArray *a) {
    return a->data && a->count > 0 && a->count <= a->max && a->max > 0;
}

// 从 src 的第 index 个位置读一个 8 字节元素
uint64_t array_get_u64(const TArray *a, int32_t index) {
    if (!array_valid(a)) return 0;
    if (index < 0 || index >= a->count) return 0;   // 边界检查
    return *(uint64_t *)(a->data + index * 8);
}

int main(void) {
    uint64_t elems[4] = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD};
    TArray a = { (uint64_t)elems, 3, 4 };   // count=3，max=4

    printf("valid=%d\n", array_valid(&a));
    for (int i = 0; i < 5; i++) {
        printf("[%d] = 0x%llX\n", i, (unsigned long long)array_get_u64(&a, i));
    }

    TArray bad = { 0, 3, 4 };
    printf("bad valid=%d\n", array_valid(&bad));
    return 0;
}