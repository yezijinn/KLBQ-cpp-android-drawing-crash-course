#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

struct Transform {
    float rot[4];
    float trans[3];
    float scale[3];
};

struct Packed48 {
    float rot[4];
    float trans[4];    // 4 个 float，凑 16 字节
    float scale[4];
};

int main(void) {
    printf("Transform: size=%zu\n", sizeof(struct Transform));
    printf("  rot=%zu trans=%zu scale=%zu\n",
        offsetof(struct Transform, rot),
        offsetof(struct Transform, trans),
        offsetof(struct Transform, scale));

    printf("Packed48: size=%zu\n", sizeof(struct Packed48));
    return 0;
}