#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    unsigned long addr = strtoul(argv[1], NULL, 16);
    int *p = (int*)addr;
    printf("读到: 0x%X\n", *p);
    return 0;
}
