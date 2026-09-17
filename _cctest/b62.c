#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *pid = argc > 1 ? argv[1] : "self";
    char path[64];
    snprintf(path, sizeof(path), "/proc/%s/maps", pid);

    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return 1; }

    char line[1024];
    printf("%-26s %-6s %s\n", "RANGE", "PERM", "PATH");
    while (fgets(line, sizeof(line), fp)) {
        unsigned long long start, end;
        char perms[8], offset[32], dev[16], inode[32], name[512] = "";

        // 格式: start-end perms offset dev inode [path]
        int n = sscanf(line, "%llx-%llx %7s %31s %15s %31s %511[^\n]",
                       &start, &end, perms, offset, dev, inode, name);
        if (n < 6) continue;

        if (strncmp(perms, "r-x", 3) == 0) {     // 只要可执行段
            printf("%08llx-%08llx %-6s %s\n", start, end, perms, name);
        }
    }
    fclose(fp);
    return 0;
}