// macro_demo.c
#include <stdio.h>

#define MAX 100
#define SQUARE(x) ((x) * (x))

int main(void) {
    int a = MAX;
    int b = SQUARE(5);
    printf("%d %d\n", a, b);   // 100 25
    return 0;
}