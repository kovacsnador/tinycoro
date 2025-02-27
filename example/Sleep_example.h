#ifndef __TINY_CORO_EXAMPLE_SLEEP_H__
#define __TINY_CORO_EXAMPLE_SLEEP_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_sleep(auto& scheduler)
{
    SyncOut() << "\n\nExample_sleep:\n";

    auto sleep = [](auto duration) -> tinycoro::Task<int32_t> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await std::suspend_always{};
        }
        co_return 42;
    };

    auto task = [&sleep]() -> tinycoro::Task<int32_t> {
        auto val = co_await sleep(1s);
        co_return val;

        // co_return co_await sleep(1s);     // or write short like this
    };

    auto val = tinycoro::GetAll(scheduler, task());

    SyncOut() << "co_return => " << *val << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_SLEEP_H__