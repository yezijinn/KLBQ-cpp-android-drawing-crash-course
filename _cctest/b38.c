void inc(int x) { x++; }

int main(void) {
    int a = 10;
    inc(a);
    printf("%d\n", a);   // 还是 10
}