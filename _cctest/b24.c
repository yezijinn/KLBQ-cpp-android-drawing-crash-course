#include <stdio.h>

struct Player {
    float x, y, z;
    int   hp;
};

int main(void) {
    printf("sizeof(struct Player) = %zu\n", sizeof(struct Player));
    return 0;
}