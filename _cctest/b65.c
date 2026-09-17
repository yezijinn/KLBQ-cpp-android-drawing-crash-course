#include <stdio.h>

int main(void) {
    if (0.1 + 0.2 == 0.3)
        printf("相等\n");
    else
        printf("不相等！\n");
    printf("%.20f\n", 0.1 + 0.2);
    return 0;
}