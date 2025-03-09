#ifndef __TINY_CORO_EXAMPLE_ANY_OF_VOID_H__
#define __TINY_CORO_EXAMPLE_ANY_OF_VOID_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_AnyOfVoid(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOfVoid:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{};
        }
    };

    std::stop_source source;

    tinycoro::AnyOfWithStopSource(scheduler, source, task1(1s), task1(2s), task1(3s));

    SyncOut() << "co_return => void" << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ANY_OF_VOID_H__