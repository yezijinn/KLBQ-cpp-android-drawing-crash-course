int main(void) {
    int a = 1;        // a 的作用域：从这里到 main 的 }

    {
        int b = 2;    // b 的作用域：从这个 { 到配对的 }
        printf("%d %d\n", a, b);   // 都能用
    }
    // printf("%d\n", b);  // 错！b 已经出了自己的 {}

    printf("%d\n", a);    // a 还能用
    return 0;
}