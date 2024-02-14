#include "thread_pool.hpp"
#include "fast_thread_pool.hpp"
#include "timer.hpp"
#include <iostream>
#include <list>
#include <algorithm>
#include <vector>
#include <chrono>
#include <random>

template<typename Pool>
void run_tests(Pool tp) {
    int x = 69;
    // std::future<int> ft = tp.push_task([](int x) { return x * x; }, 69);
    std::future<int> ft = tp.push_task([&]() { return x * x; });
    tp.manual_run_task();
    ft.get();
}

template<typename T, typename Pool>
struct sorter
{
    Pool pool;
    
    std::list<T> do_sort(std::list<T>& chunk_data)
    {
        if(chunk_data.empty())
        {
            return chunk_data;
        }
        
        std::list<T> result;
        result.splice(result.begin(),chunk_data,chunk_data.begin());
        T const& partition_val=*result.begin();
    
        typename std::list<T>::iterator divide_point=
            std::partition(
                chunk_data.begin(),chunk_data.end(),
                [&](T const& val){return val<partition_val;});

        std::list<T> new_lower_chunk;
        new_lower_chunk.splice(
            new_lower_chunk.end(),
            chunk_data,chunk_data.begin(),
            divide_point);

        std::future<std::list<T> > new_lower=
            pool.push_task(
                std::bind(
                    &sorter::do_sort,this,
                    std::move(new_lower_chunk)));
    
        std::list<T> new_higher(do_sort(chunk_data));
        
        result.splice(result.end(),new_higher);
        while(new_lower.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            pool.manual_run_task();
        }
        
        result.splice(result.begin(),new_lower.get());
        return result;
    }
};


template<typename T, typename Pool>
std::list<T> parallel_quick_sort(std::list<T> input)
{
    if(input.empty())
    {
        return input;
    }
    sorter<T, Pool> s;
    
    return s.do_sort(input);

}

std::list<int> generateRandom() {
    constexpr int LIM = (int)5000;
    std::list<int> ret;
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> dist(-LIM, LIM);
    for (int i = 0; i < LIM; i++) {
        ret.push_back(dist(rng));
    }
    return ret;
}

long long tot = 0;

template<typename Pool>
void benchmark_quicksort(Pool tp, std::list<int> to_sort) {
    to_sort = parallel_quick_sort<int, Pool>(std::move(to_sort));
    tot += *to_sort.begin(); // Stop compiler optimizations
}

void benchmark_singlethread_sort(std::list<int> to_sort) {
    to_sort.sort();
    tot += *to_sort.begin(); // Stop compiler optimizations
}

int main(void) {
    run_tests(chronicles::thread_pool());
    run_tests(chronicles::fast_thread_pool());

    qnd::timer timer("../stats/thread_pool.txt");
    for (int i = 0; i < 100; i++) {
        std::cout << "at: " << i << std::endl;
        std::list<int> to_sort = generateRandom();
        timer.start();
        benchmark_quicksort(chronicles::thread_pool(), std::move(to_sort));
        timer.stop();
    }
    timer.printStats();
    timer.reset("../stats/fast_thread_pool.txt");
    for (int i = 0; i < 100; i++) {
        std::cout << "at: " << i << std::endl;
        std::list<int> to_sort = generateRandom();
        timer.start();
        benchmark_quicksort(chronicles::fast_thread_pool(), std::move(to_sort));
        timer.stop();
    }
    timer.printStats();
    timer.reset("../stats/singlethread_sort.txt");
    for (int i = 0; i < 100; i++) {
        std::cout << "at: " << i << std::endl;
        std::list<int> to_sort = generateRandom();
        timer.start();
        benchmark_singlethread_sort(std::move(to_sort));
        timer.stop();
    }
    timer.printStats();
    std::cout << tot << std::endl;
    return 0;
}