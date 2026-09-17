int x = 1;            // 全局 x

int main(void) {
    int x = 2;        // 局部 x，遮蔽了全局 x
    printf("%d\n", x);  // 输出 2（局部优先）
    return 0;
}