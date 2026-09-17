#include <stdio.h>

int main(void) {
    int arr[4] = {10, 20, 30, 40};
    int *p = arr;              // p 指向首元素

    for (int i = 0; i < 4; i++) {
        printf("arr[%d]=%d  *(p+%d)=%d\n",
               i, arr[i], i, *(p + i));
    }
    return 0;
}