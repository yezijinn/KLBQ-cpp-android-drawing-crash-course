#define MAX 100

void f(void) {
    int a = MAX;   // 100
}

int main(void) {
    int b = MAX;   // 100（依然有效）
    return 0;
}