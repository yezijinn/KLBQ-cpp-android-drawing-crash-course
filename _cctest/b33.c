int main(void) {
    int r = add(3, 4);       // 编译到这里，编译器还不认识 add
    return 0;
}

int add(int a, int b) {      // 定义在这里（太晚了）
    return a + b;
}