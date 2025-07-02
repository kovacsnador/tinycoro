#ifndef __TINY_CORO_EXAMPLE_ASYNC_PULLING_H__
#define __TINY_CORO_EXAMPLE_ASYNC_PULLING_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_asyncPulling(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncPulling:\n";

    auto asyncTask = [](int32_t i) -> tinycoro::Task<int32_t> {
        SyncOut() << "  asyncTask... Thread id: " << std::this_thread::get_id() << '\n';
        auto future = std::async(
            std::launch::async,
            [](auto i) {
                // simulate some work
                std::this_thread::sleep_for(1s);
                return i * i;
            },
            i);

        // simple old school pulling
        while (future.wait_for(0s) != std::future_status::ready)
        {
            co_await std::suspend_always{};
        }

        auto res = future.get();

        SyncOut() << "  asyncTask return: " << res << " , Thread id : " << std::this_thread::get_id() << '\n';
        co_return res;
    };

    auto task = [&asyncTask]() -> tinycoro::Task<int32_t> {
        SyncOut() << "  task... Thread id: " << std::this_thread::get_id() << '\n';
        auto val = co_await asyncTask(4);
        co_return val;
    };

    auto val = tinycoro::AllOf(scheduler, task());
    SyncOut() << "co_return => " << *val << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_PULLING_H__