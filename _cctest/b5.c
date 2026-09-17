#include <stdio.h>

int main(void) {
    int target = 42;         // 目标值（实际游戏里应该随机，这里先写死）
    int guess = 0;
    int count = 0;

    printf("猜一个整数：\n");
    while (guess != target) {
        printf("请输入：");
        scanf("%d", &guess);  // 从键盘读一个整数（& 是取地址，第 05 章讲）
        count++;

        if (guess < target) {
            printf("太小了\n");
        } else if (guess > target) {
            printf("太大了\n");
        } else {
            printf("恭喜！你用了 %d 次\n", count);
        }
    }
    return 0;
}