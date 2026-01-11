#ifndef __TINY_CORO_EXAMPLE_CO_YIELD_FOR_TASK_H__
#define __TINY_CORO_EXAMPLE_CO_YIELD_FOR_TASK_H__

#include <ranges>

#include <tinycoro/tinycoro_all.h>

tinycoro::Task<int32_t> Producer(int32_t max)
{
    for (auto it : std::views::iota(0, max))
    {
        co_yield it;    // yield a value
    }
    co_return max;  // return the max value
}

tinycoro::Task<void> Consumer(int32_t max)
{
    // create the corouitne task
    auto producer = Producer(max);
    
    int32_t v{};
    for (auto it : std::views::iota(0, max))
    {
        v = co_await producer; // result from co_yield
        assert(v == it);
    }
    v = co_await producer; // Final result from co_return
    assert(v == max);
}

void Example_coyieldForTask()
{
    SyncOut() << "\n\nExample_coyieldForTask:\n";

    tinycoro::AllOf(Consumer(42));
}

#endif // __TINY_CORO_EXAMPLE_CO_YIELD_FOR_TASK_H__