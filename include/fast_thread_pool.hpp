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


class fast_thread_pool {
public:
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

    struct StealableQueue {
        StealableQueue() = default;

        void push(FnWrapper val) {
            std::lock_guard<std::mutex> lck(mtx_steal);
            q.push_back(std::move(val));
        }

        bool try_pop(FnWrapper& val) {
            std::lock_guard<std::mutex> lck(mtx_steal);
            if (_empty()) return false;
            val = std::move(q.front());
            q.pop_front();
            return true;
        }

        bool try_steal(FnWrapper& val) {
            std::lock_guard<std::mutex> lck(mtx_steal);
            if (_empty()) return false;
            val = std::move(q.back());
            q.pop_back();
            return true;
        }

        bool empty() const {
            std::lock_guard<std::mutex> lck(mtx_steal);
            return _empty();
        }

        bool _empty() const {
            return q.empty();
        }

        mutable std::mutex mtx_steal;
        std::deque<FnWrapper> q;
    };

public:

    /* CONSTRUCTORS */

    fast_thread_pool(unsigned int init_threads = std::thread::hardware_concurrency()) : end_work{false} {
        try {
            for (unsigned int i = 0; i < init_threads; i++) {
                queues.push_back(std::make_unique<StealableQueue>());
            }
            for (unsigned int i = 0; i < init_threads; i++) {
                threads.emplace_back(std::thread(&fast_thread_pool::execute_task, this, i));
            }
        } catch(...) {
            end_work = true;
            throw;
        }
    }

    ~fast_thread_pool() {
        end_work.store(true);
        // cv.notify_all();
        for (auto& t : threads) {
            if (t.joinable())
                t.join();
        }
    }

    fast_thread_pool(const fast_thread_pool &other) = delete;
    fast_thread_pool& operator=(const fast_thread_pool &other) = delete;

    /* MEMBER FUNCTIONS */

    template<typename F>
    auto push_task(F&& func) -> std::future<decltype(func())> {
        using RetType = decltype(func());
        using Package = std::packaged_task<RetType()>;
        Package pt(func);
        std::future<RetType> ft = pt.get_future();
        FnWrapper wrapped_package(std::move(pt));
        // If current thread is a worker
        if (local_q) {
            local_q->push([my_package = std::move(wrapped_package)]() mutable {
                my_package();
            });
        } else {
            std::lock_guard<std::mutex> lck(mtx_q);
            task_q.emplace([my_package = std::move(wrapped_package)]() mutable {
                my_package();
            });
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
        bool run = false;
        // There is a queue on this thread
        if (local_q && !local_q->empty()) {
            if (local_q->try_pop(func)) run = true;
        } else {
            std::unique_lock<std::mutex> lck(mtx_q);
            if (!task_q.empty()) {
                func = std::move(task_q.front());
                task_q.pop();
                run = true;
            } else if (local_q) {
                lck.unlock();
                if (try_work_steal(func)) {
                    run = true;
                }
            }
        }
        if (run) {
            func();
        } else {
            std::this_thread::yield();
        }
    }

private:

    /* HELPER FUNCTIONS */

    bool try_work_steal(FnWrapper& val) {
        for (unsigned int i = 0; i < queues.size(); i++) {
            unsigned int idx = (i + curr_ct + 1) % queues.size();
            if (queues[idx]->try_steal(val)) {
                return true;
            }
        }
        return false;
    }

    void execute_task(unsigned int index) {
        curr_ct = index;
        local_q = queues[index].get();
        while (!end_work) {
            manual_run_task();
        }
    }

private:

    /* MEMBER VARIABLES */

    std::atomic<bool> end_work;
    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<StealableQueue>> queues;
    std::mutex mtx_q;
    std::condition_variable cv;
    std::queue<FnWrapper> task_q;

    static thread_local StealableQueue *local_q;
    static thread_local unsigned int curr_ct;
};

}

thread_local chronicles::fast_thread_pool::StealableQueue* chronicles::fast_thread_pool::local_q{nullptr};
thread_local unsigned int chronicles::fast_thread_pool::curr_ct{0};