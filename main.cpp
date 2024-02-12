#include <iostream>
#include "thread_pool.hpp"
#include "fast_thread_pool.hpp"

template<typename Pool>
void run_tests(Pool tp) {
    int x = 69;
    // std::future<int> ft = tp.push_task([](int x) { return x * x; }, 69);
    std::future<int> ft = tp.push_task([&]() { return x * x; });
    tp.manual_run_task();
    std::cout << ft.get() << std::endl;
}

int main(void) {
    run_tests(chronicles::thread_pool());
    run_tests(chronicles::fast_thread_pool());
    return 0;
}