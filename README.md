# chronipools
C++ High Performance Thread Pool.

Just drag and drop include/thread_pool.hpp into your own project and it should
work. It is under the namespace `chronicles::thread_pool` and `chronicles::fast_thread_pool`.

I probably wouldn't use this in a production environment, but it should be
helpful if you are looking to see the general structure of a thread pool,
this one is very readable and you should be able to learn something from it.
There are also comments for choices that might not seem entirely obvious.

Also note: there are two thread pools - they have a similar API but the fast
thread pool has a separate local queue for each thread, whilst the normal
thread pool has a global work queue. If a thread pool is under high contention,
the global queue performance may degrade, so that is the reason for the fast thread pool.
However, the fast pool does use more memory and there is overhead as well for assigning tasks
so I recommend only using the fast one if you are planning to push a lot of tasks
constantly.

# API

- Default constructor will use hardware_concurrency() to generate a maximum
amount of threads

- Submit task (push_task) will send a task into a queue, which will give you
a future back that you can use later.

```
#include <future>

std::future<int> ft = tp.push_task([]{ return 32; });
int res = ft.get(); // <-- will be 32, after the thread pool executes function.
```

