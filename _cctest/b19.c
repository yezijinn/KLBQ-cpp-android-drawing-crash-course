#include <stdio.h>

int main(void) {
    int scores[6] = {90, 85, 88, 92, 78, 95};
    int n = (int)(sizeof(scores) / sizeof(scores[0]));

    // 求总分
    int sum = 0;
    for (int i = 0; i < n; i++) {
        sum = sum + scores[i];
    }
    printf("总分 = %d\n", sum);

    // 求平均
    float avg = (float)sum / n;   // 转成 float 才能得到小数
    printf("平均 = %.1f\n", avg);

    // 找最高分
    int max = scores[0];
    for (int i = 1; i < n; i++) {
        if (scores[i] > max) {
            max = scores[i];
        }
    }
    printf("最高分 = %d\n", max);

    return 0;
}