#include <cassert>
#include <cmath>
#include <cstdio>

static bool Near(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

int main(void) {
    // 每个行为一条断言
    assert(Near(Vec3{3,4,0}.Length(), 5.0f));
    printf("通过：长度\n");
    return 0;
}