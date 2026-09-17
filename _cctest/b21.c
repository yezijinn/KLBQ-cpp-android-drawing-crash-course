#include <stdio.h>

int main(void) {
    int scores[3] = {90, 85, 88};
    int guard = 12345;

    printf("guard = %d\n", guard);
    scores[3] = 999;                    // 越界！只有 0,1,2 合法
    printf("guard = %d\n", guard);      // 可能变成 999
    return 0;
}