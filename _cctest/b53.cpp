#include <cstdio>

int main() {
    int a = 10;
    int &r = a;        // r 是 a 的别名

    printf("a=%d, r=%d\n", a, r);    // 10 10

    r = 20;                            // 改 r
    printf("a=%d, r=%d\n", a, r);    // 20 20（a 也变了）

    a = 30;                            // 改 a
    printf("a=%d, r=%d\n", a, r);    // 30 30（r 也变了）

    printf("&a=%p, &r=%p\n", (void*)&a, (void*)&r);
    // 打印出来是同一个地址！证明 r 就是 a

    return 0;
}