void counter(void) {
    static int count = 0;    // 只初始化一次（第一次调用时）
    count++;
    printf("调用了 %d 次\n", count);
}

int main(void) {
    counter();   // 调用了 1 次
    counter();   // 调用了 2 次
    counter();   // 调用了 3 次
    return 0;
}