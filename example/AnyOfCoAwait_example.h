#ifndef __TINY_CORO_EXAMPLE_ANY_OF_COAWAIT_H__
#define __TINY_CORO_EXAMPLE_ANY_OF_COAWAIT_H__

#include <string>

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

tinycoro::Task<void> Example_AnyOfCoAwait(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOfCoAwait:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{};
        }
        co_return count;
    };

    // Nonblocking wait for other tasks
    auto [t1, t2, t3] = co_await tinycoro::AnyOfAwait(scheduler, task1(1s), task1(2s), task1(3s));

    SyncOut() << "co_return => " << *t1 << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ANY_OF_COAWAIT_H__