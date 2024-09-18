#ifndef __TINY_CORO_EXAMPLE_ANY_OF_VOID_EXCEPTION_H__
#define __TINY_CORO_EXAMPLE_ANY_OF_VOID_EXCEPTION_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_AnyOfException(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOfException:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend<void>{};
        }
    };

    auto task2 = [](auto duration) -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            SyncOut() << "  Throwing exception\n";
            throw std::runtime_error("Exception throwed!");
            co_await tinycoro::CancellableSuspend<void>{};
        }
        co_return 42;
    };

    try
    {
        [[maybe_unused]] auto results = tinycoro::AnyOf(scheduler, task1(1s), task1(2s), task1(3s), task2(5s));
    }
    catch (const std::exception& e)
    {
        SyncOut() << "Exception: " << e.what() << '\n';
    }
}

#endif //!__TINY_CORO_EXAMPLE_ANY_OF_VOID_EXCEPTION_H__