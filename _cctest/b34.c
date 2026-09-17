int add(int a, int b);       // ← 前向声明：先告诉编译器"有这么个函数"

int main(void) {
    int r = add(3, 4);       // OK 了
    return 0;
}

int add(int a, int b) {
    return a + b;
}