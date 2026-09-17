#include <stdio.h>

void recurse(int n) {
    char buf[1024];              // 每层用掉 1KB 栈
    printf("depth=%d  buf@%p\n", n, (void*)buf);
    recurse(n + 1);
}

int main(void) { recurse(0); }