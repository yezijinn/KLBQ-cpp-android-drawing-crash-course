#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstdio>

constexpr int N = 1000000;

// 1. 无锁（会有竞争，结果错误，只测速度）
volatile int counter1 = 0;
// 2. 互斥锁
int counter2 = 0; std::mutex mtx;
// 3. 自旋锁
int counter3 = 0; std::atomic<unsigned char> spinFlag{0};
// 4. 原子
std::atomic<int> counter4{0};

void bench(const char *name, const std::function<void()> &fn) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    printf("%-10s : %6.1f ms\n", name,
           std::chrono::duration<double, std::milli>(t1-t0).count());
}

int main(void) {
    bench("无锁", []{
        for (int i = 0; i < N; i++) counter1++;
    });
    bench("互斥锁", []{
        for (int i = 0; i < N; i++) { std::lock_guard<std::mutex> l(mtx); counter2++; }
    });
    bench("自旋锁", []{
        for (int i = 0; i < N; i++) {
            while (spinFlag.exchange(1)) { }
            counter3++;
            spinFlag.store(0);
        }
    });
    bench("原子", []{
        for (int i = 0; i < N; i++) counter4++;
    });
    return 0;
}