#include <iostream>
#include "thread_pool.hpp"

int main(void) {
    chronicles::thread_pool tp;
    int x = 69;
    // std::future<int> ft = tp.push_task([](int x) { return x * x; }, 69);
    std::future<int> ft = tp.push_task([&]() { return x * x; });
    tp.manual_run_task();
    std::cout << ft.get() << std::endl;
    return 0;
}