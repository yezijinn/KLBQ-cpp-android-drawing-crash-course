#include <stdio.h>

void level3(void) { int x; printf("level3  &x = %p\n", (void*)&x); }
void level2(void) { int x; printf("level2  &x = %p\n", (void*)&x); level3(); }
void level1(void) { int x; printf("level1  &x = %p\n", (void*)&x); level2(); }

int main(void) {
    int x;
    printf("main    &x = %p\n", (void*)&x);
    level1();
    return 0;
}