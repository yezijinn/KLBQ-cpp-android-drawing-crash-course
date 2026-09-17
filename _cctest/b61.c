#include <stdio.h>

int main(void) {
    FILE *fp = fopen("/proc/self/maps", "r");
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        printf("%s", line);       // 直接打印，观察各段
    }
    fclose(fp);
    return 0;
}