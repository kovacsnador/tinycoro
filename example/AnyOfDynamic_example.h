#ifndef __TINY_CORO_EXAMPLE_ANY_OF_DYNAMIC_H__
#define __TINY_CORO_EXAMPLE_ANY_OF_DYNAMIC_H__

#include <tinycoro/tinycoro_all.h>

#include <vector>

#include "Common.h"

void Example_AnyOfDynamic(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOfDynamic:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{};
        }
        co_return count;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1(1s));
    tasks.push_back(task1(2s));
    tasks.push_back(task1(3s));

    auto results = tinycoro::AnyOf(scheduler, tasks);

    SyncOut() << "co_return => " << results[0].value() << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ANY_OF_DYNAMIC_H__