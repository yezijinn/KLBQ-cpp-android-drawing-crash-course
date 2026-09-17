#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct Level { uint64_t actors[3]; int count; };
struct World { struct Level *level; };

int main(void) {
    struct Level lv = {{0x1111, 0x2222, 0x3333}, 3};
    struct World w = {&lv};

    // 模拟"基址"
    uint64_t base = (uint64_t)&w;

    // 第一跳：base + 0 → level 指针
    uint64_t level_addr = *(uint64_t *)(base + 0);
    // 第二跳：level + 0 → actors 数组首地址（就在结构体开头）
    uint64_t first = *(uint64_t *)(level_addr + 0);
    int count = *(int *)(level_addr + 24);   // 3 个 uint64 = 24 字节后是 count

    printf("level=0x%llX first=0x%llX count=%d\n",
           (unsigned long long)level_addr,
           (unsigned long long)first, count);
    return 0;
}