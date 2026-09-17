#include <thread>
#include <cstdio>

int counter = 0;

void increment() {
    for (int i = 0; i < 100000; i++)
        counter++;          // 三个线程同时做
}

int main(void) {
    std::thread t1(increment), t2(increment), t3(increment);
    t1.join(); t2.join(); t3.join();
    printf("counter = %d (期望 300000)\n", counter);
    return 0;
}