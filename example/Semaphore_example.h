#ifndef __TINY_CORO_EXAMPLE_SEMAPHORE_H__
#define __TINY_CORO_EXAMPLE_SEMAPHORE_H__

#include <tinycoro/tinycoro_all.h>

void Example_Semaphore(tinycoro::Scheduler& scheduler)
{
    tinycoro::Semaphore<1> semaphore;

    int32_t count{0};

    auto task = [&semaphore, &count]() -> tinycoro::Task<int32_t> {
        auto lock = co_await semaphore;
        co_return ++count;
    };

    auto [c1, c2, c3] = tinycoro::AllOf(scheduler, task(), task(), task());

    // Every varaible should have unique value (on intel processor for sure :) ).
    // So (c1 != c2 && c2 != c3 && c3 != c1)
    // possible output: c1 == 1, c2 == 2, c3 == 3

    std::cout << *c1 << " " << *c2 << " " << *c3 << '\n';
}

#endif