# chronipools
C++ High Performance Thread Pool.

Just drag and drop include/thread_pool.hpp into your own project and it should
work. It is under the namespace `chronicles::thread_pool`.

I probably wouldn't use this in a production environment, but it should be
helpful if you are looking to see the general structure of a thread pool,
this one is very readable and you should be able to learn something from it.

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

