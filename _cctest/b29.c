#include <stdio.h>

int read_guess(void) {
    int g;
    printf("请输入：");
    scanf("%d", &g);
    return g;
}

void check_guess(int guess, int target, int count) {
    if (guess < target)       printf("太小了\n");
    else if (guess > target)  printf("太大了\n");
    else                      printf("恭喜！你用了 %d 次\n", count);
}

int main(void) {
    int target = 42;
    int count = 0;
    int guess = 0;
    while (guess != target) {
        guess = read_guess();
        count++;
        check_guess(guess, target, count);
    }
    return 0;
}