#ifndef __TINY_CORO_EXAMPLE_ANY_OF_H__
#define __TINY_CORO_EXAMPLE_ANY_OF_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_AnyOf(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOf:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{};
        }
        co_return count;
    };

    auto [t1, t2, t3] = tinycoro::AnyOf(scheduler, task1(1s), task1(2s), task1(3s));


    SyncOut() << "co_return => " << *t1 << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ANY_OF_H__