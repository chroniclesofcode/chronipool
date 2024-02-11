#include <iostream>
#include <mutex>
#include <thread>
#include <future>
#include <vector>
#include <functional>
#include <queue>

namespace chronicles {

class thread_pool {
public:
    /* CONSTRUCTORS */

    thread_pool() {
    }

    thread_pool(int init_threads) {

    }

    /* MEMBER FUNCTIONS */

    template<typename Ret, typename... Args>
    auto push_task(Ret func, Args&&... args) -> std::future<decltype(func(std::forward<Args...>(args...)))> {
        using RetType = decltype(func(std::forward<Args...>(args...)));
        std::packaged_task<RetType(Args...)> pt(func);
        std::future<RetType> ft = pt.get_future();
        pt(std::forward<Args...>(args...));
        return ft;
    }

private:

    /* HELPER FUNCTIONS */

    void execute_task() {

    }

private:

    /* MEMBER VARIABLES */

    std::vector<std::thread> threads;
    std::mutex mtx_q;
};

}