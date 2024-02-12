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
private:

    /* HELPER CLASS DEFINITION */

    // we will have a pointer of generic type to a derived class (of generic
    // type) which will hide it's template type, so that we can use it
    // anywhere a std::function can be used. This is to escape the rule that
    // std::functions can only be of copy-constructible types.
    struct FnWrapper {

        struct generic_type {
            virtual void call() = 0;
            virtual ~generic_type() {}
        };

        template <typename T>
        struct erased_type : generic_type {
            T elem;
            erased_type(T&& other) : elem{std::move(other)} {}
            void call() { elem(); }
        };

        std::unique_ptr<generic_type> curr;

        // FnWrapper can take in any type (via universal reference) and store
        // it as a pointer to call.
        template <typename F>
        FnWrapper(F&& other) : curr(new erased_type<F>(std::move(other))) {}
        FnWrapper() = default;
        
        void operator()(){ curr->call(); }

        // We take their unique_ptr as ours
        FnWrapper(FnWrapper&& other) : curr{std::move(other.curr)} {}
        
        FnWrapper& operator=(FnWrapper&& other) {
            curr = std::move(other.curr);
            return *this; 
        }

        FnWrapper(const FnWrapper& other) = delete;
    };

public:

    /* CONSTRUCTORS */

    thread_pool(unsigned int init_threads = std::thread::hardware_concurrency()) : end_work{false} {
        try {
            for (unsigned int i = 0; i < init_threads; i++) {
                threads.emplace_back(std::thread(&thread_pool::execute_task, this));
            }
        } catch(...) {
            end_work = true;
            throw;
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
        Package pt(func);
        std::future<RetType> ft = pt.get_future();
        FnWrapper wrapped_package(std::move(pt));
        {
            std::lock_guard<std::mutex> lck(mtx_q);
            
            task_q.emplace([my_package = std::move(wrapped_package)]() mutable {
                my_package();
            });
            
            cv.notify_one();
        }
        return ft;
    }

    // When implementing parallel algorithms, one may encounter a case where
    // they are waiting for another task to complete - however, there may be deadlock
    // if all threads are actually at this 'waiting' stage, with no threads
    // remaining to do the work waited for. So we provide a manual_run_task to
    // make progress even while waiting (use carefully).
    void manual_run_task() {
        FnWrapper func;
        {
                std::unique_lock<std::mutex> lck(mtx_q);
                if (task_q.empty()) return;
                func = std::move(task_q.front());
                task_q.pop();
        }
        func();
    }

private:

    /* HELPER FUNCTIONS */

    void execute_task() {
        while (!end_work) {
            FnWrapper func;
            {
                std::unique_lock<std::mutex> lck(mtx_q);
                cv.wait(lck, [&]() { return !task_q.empty() || end_work; });
                if (end_work) break;

                func = std::move(task_q.front());
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
    std::queue<FnWrapper> task_q;
};

}