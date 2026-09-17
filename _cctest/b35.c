#include <stdio.h>

int max_of_two(int a, int b) {
    if (a > b) return a;
    return b;
}

int max_of_three(int a, int b, int c) {
    return max_of_two(max_of_two(a, b), c);
}

void print_line(int n) {
    for (int i = 0; i < n; i++) {
        printf("-");
    }
    printf("\n");
}

int is_even(int x) {
    return (x % 2 == 0) ? 1 : 0;   // ? : 是三元运算符，第 03 章讲过
}

int main(void) {
    printf("max(3, 7) = %d\n", max_of_two(3, 7));
    printf("max(3, 7, 5) = %d\n", max_of_three(3, 7, 5));
    print_line(20);
    printf("is_even(8) = %d, is_even(7) = %d\n", is_even(8), is_even(7));
    return 0;
}