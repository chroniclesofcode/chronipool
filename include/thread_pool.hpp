#include <iostream>
#include <mutex>
#include <thread>
#include <future>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <functional>
#include <queue>
#include <memory>

namespace chronicles {

class thread_pool {
public:

    /* CONSTRUCTORS */

    thread_pool() : end_work{0} {
        for (int i = 0; i < std::thread::hardware_concurrency(); i++) {
            threads.emplace_back(std::thread(&thread_pool::execute_task, this));
        }
    }

    thread_pool(int init_threads) : end_work{0} {
        for (int i = 0; i < init_threads; i++) {
            threads.emplace_back(std::thread(&thread_pool::execute_task, this));
        }
    }

    ~thread_pool() {
        end_work.store(true);
        cv.notify_all();
        for (auto& t : threads) {
            if (t.joinable())
                t.join();
        }
    }

    thread_pool(const thread_pool &other) = delete;
    thread_pool& operator=(const thread_pool &other) = delete;

    /* MEMBER FUNCTIONS */

    template<typename F>
    auto push_task(F&& func) -> std::future<decltype(func())> {
        using RetType = decltype(func());
        using Package = std::packaged_task<RetType()>;
        std::shared_ptr<Package> spt = std::make_shared<Package>(func);
        std::future<RetType> ft = spt->get_future();
        {
            std::lock_guard<std::mutex> lck(mtx_q);
            task_q.emplace([my_package = spt]() {
                (*my_package)();
            });
            cv.notify_one();
        }
        return ft;
    }

    /*
    template<typename Ret, typename... Args>
    auto push_task(Ret func, Args&&... args) -> std::future<decltype(func(std::forward<Args>(args)...))> {
        using RetType = decltype(func(std::forward<Args>(args)...));
        std::packaged_task<RetType(Args...)> pt(func);
        std::future<RetType> ft = pt.get_future();
        {
            std::lock_guard lck(mtx_q);
            task_q.emplace([my_pt = std::move(pt), ...prms = std::forward<Args>(args)]() {
                my_pt(prms...);
            });
        }
        //pt(std::forward<Args>(args)...);
        return ft;
    }
    */

private:

    /* HELPER FUNCTIONS */

    void execute_task() {
        while (!end_work) {
            std::function<void()> func;
            {
                std::unique_lock<std::mutex> lck(mtx_q);
                cv.wait(lck, [&]() { return !task_q.empty() || end_work; });
                if (end_work) break;

                func = task_q.front();
                task_q.pop();
            }
            func();
        }
    }

private:

    /* MEMBER VARIABLES */

    std::atomic<bool> end_work;
    std::vector<std::thread> threads;
    std::mutex mtx_q;
    std::condition_variable cv;
    std::queue<std::function<void()>> task_q;
};

}