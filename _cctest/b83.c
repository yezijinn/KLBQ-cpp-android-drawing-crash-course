// b.c：试着读 a 的地址
#include <stdio.h>

int main(int argc, char **argv) {
    unsigned long addr = strtoul(argv[1], nullptr, 16);
    int *p = (int*)addr;
    printf("读到: 0x%X\n", *p);      // 会怎样？
    return 0;
}