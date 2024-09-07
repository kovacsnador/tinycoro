#ifndef __TINY_CORO_EXAMPLE_WITH_STOP_TOKEN_H__
#define __TINY_CORO_EXAMPLE_WITH_STOP_TOKEN_H__

#include <tinycoro/tinycoro_all.h>

#include <stop_token>

#include "Common.h"

void Example_usageWithStopToken(auto& scheduler)
{
    SyncOut() << "\n\nExample_usageWithStopToken:\n";

    auto task1 = [](auto duration, std::stop_source& source) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await std::suspend_always{};
        }
        source.request_stop();
    };

    auto task2 = [](std::stop_token token) -> tinycoro::Task<int32_t> {
        auto sleep = [](auto duration) -> tinycoro::Task<void> {
            for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
            {
                co_await std::suspend_always{};
            }
        };

        int32_t result{};
        while (token.stop_requested() == false)
        {
            ++result;
            co_await sleep(100ms);
        }
        co_return result;
    };

    std::stop_source source;

    auto futures = scheduler.EnqueueTasks(task1(1s, source), task2(source.get_token()));

    auto results = tinycoro::GetAll(futures);

    auto task2Val = std::get<1>(results);

    SyncOut() << "co_return => " << task2Val << '\n';
}


#endif //!__TINY_CORO_EXAMPLE_WITH_STOP_TOKEN_H__