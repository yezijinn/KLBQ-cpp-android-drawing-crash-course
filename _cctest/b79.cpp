#include <cstdio>
#include <cmath>
#include <cstdint>
#include <initializer_list>   // for (float b : {…}) 需要它

void show_spacing(float base) {
    float next = std::nextafter(base, base * 2 + 100);
    printf("在 %.0f 附近，float 最小间隔 = %.6f\n", base, next - base);
}

int main(void) {
    for (float b : {1.0f, 100.0f, 10000.0f, 1e6f, 1e7f, 1e8f})
        show_spacing(b);

    // 大数吃小数
    float big = 1e7f;
    float before = big;
    big += 0.05f;
    printf("\n1e7 + 0.05 结果变化: %s\n",
           (big == before) ? "无变化（被吃掉）" : "有变化");

    // 抵消误差
    float a = 1.2345678f, b = 1.2345677f;
    printf("1.2345678 - 1.2345677 = %.10f (真值 0.0000001)\n", a - b);

    // 累积误差
    float sum = 0;
    for (int i = 0; i < 1000000; i++) sum += 0.1f;
    printf("100万次 += 0.1f: %.2f (真值 100000.00)\n", sum);

    double dsum = 0;
    for (int i = 0; i < 1000000; i++) dsum += 0.1;
    printf("用 double 累加: %.2f\n", dsum);

    // NaN/inf
    float nan_ = 0.0f / 0.0f;
    float inf  = 1.0f / 0.0f;
    printf("\nnan == nan ? %d\n", nan_ == nan_);
    printf("inf - inf = %f\n", inf - inf);
    printf("isfinite(nan)=%d isfinite(inf)=%d isfinite(1.0)=%d\n",
           std::isfinite(nan_), std::isfinite(inf), std::isfinite(1.0f));
    return 0;
}