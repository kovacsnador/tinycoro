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

            // do some work
            std::this_thread::sleep_for(100ms);
        };

        // wait with return value
        co_await tinycoro::AsyncCallbackAwaiter([](auto wrappedCallback) { AsyncCallbackAPIvoid(wrappedCallback, nullptr); }, cb);
        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_H__