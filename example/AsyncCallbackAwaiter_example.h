#ifndef __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_H__
#define __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_asyncCallbackAwaiter(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        auto cb = []([[maybe_unused]] void* userData, int i) {
            SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';

            throw std::runtime_error{"error"};

            // do some work
            std::this_thread::sleep_for(100ms);
        };

        try
        {
            // wait with return value
            co_await tinycoro::AsyncCallbackAwaiter([](auto wrappedCallback) { AsyncCallbackAPIvoid(wrappedCallback, nullptr); }, cb);
        }
        catch (const std::exception& e)
        {
            std::cerr << e.what() << '\n';
        }
        co_return 42;
    };

    try
    {
        auto val = tinycoro::AllOf(scheduler, task());
        SyncOut() << "co_return => " << *val << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    // auto val = tinycoro::AllOf(scheduler, task());
    // SyncOut() << "co_return => " << *val << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_H__