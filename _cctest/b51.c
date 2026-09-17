#include <stdio.h>
#include <stdint.h>

int main(void) {
    FILE *fp = fopen("data.bin", "rb");
    if (fp == NULL) { printf("打不开文件\n"); return 1; }

    // 求文件大小
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    printf("文件大小: %ld 字节\n", size);

    // 读前 16 字节
    uint8_t buf[16];
    size_t n = fread(buf, 1, 16, fp);
    printf("读到 %zu 字节\n", n);

    fclose(fp);
    return 0;
}