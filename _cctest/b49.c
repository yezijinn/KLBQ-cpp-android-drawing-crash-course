#include <stdio.h>

#define SQUARE_BAD(x) x * x
#define SQUARE_OK(x) ((x) * (x))
#define LOG(fmt, ...) printf("[%s] " fmt "\n", __func__, ##__VA_ARGS__)

int main(void) {
    printf("bad=%d ok=%d\n", SQUARE_BAD(1+2), SQUARE_OK(1+2));
    LOG("value=%d", 42);
    return 0;
}