#ifndef __TINY_CORO_EXAMPLE_SLEEP_H__
#define __TINY_CORO_EXAMPLE_SLEEP_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_sleep(auto& scheduler)
{
    SyncOut() << "\n\nExample_sleep:\n";

    tinycoro::SoftClock clock;

    auto task = [&]() -> tinycoro::Task<int32_t> {
        // simple coroutine friendly sleep here
        co_await tinycoro::SleepFor(clock, 1s);
        co_return 42;
    };

    auto val = tinycoro::GetAll(scheduler, task());

    SyncOut() << "co_return => " << *val << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_SLEEP_H__