#include <cstdio>
#include <vector>

int main() {
    std::vector<int> v;

    // 加 5 个元素
    for (int i = 1; i <= 5; i++) {
        v.push_back(i * 10);
    }

    printf("size = %zu\n", v.size());   // 5

    // 遍历打印
    for (int x : v) {
        printf("%d ", x);
    }
    printf("\n");   // 10 20 30 40 50

    // 用下标改
    v[0] = 999;
    printf("v[0] = %d\n", v[0]);   // 999

    // 求和
    int sum = 0;
    for (int x : v) sum += x;
    printf("sum = %d\n", sum);

    return 0;
}