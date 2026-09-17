void f(int x) {
    int y = x * 2;      // x 和 y 都是 f 的"局部的东西"
    printf("%d\n", y);
}

int main(void) {
    f(10);
    // printf("%d\n", x);  // 错！x 在这里不存在
    // printf("%d\n", y);  // 错！y 在这里也不存在
    return 0;
}