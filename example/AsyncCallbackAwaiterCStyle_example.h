#ifndef __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_H__
#define __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_asyncCallbackAwaiter_CStyle(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_CStyle:\n";

    auto task = []() -> tinycoro::Task<int32_t> {

        SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        auto cb = [](void* userData, int i) {
            SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';

            auto d = tinycoro::UserData::Get<int>(userData); 
            *d = 21;
        };

        auto async = [](auto wrappedCallback, void* wrappedUserData) { AsyncCallbackAPIvoid(wrappedCallback, wrappedUserData); return 21; };
        
        int userData{0};

        auto res = co_await tinycoro::AsyncCallbackAwaiter_CStyle(async, cb, tinycoro::IndexedUserData<0>(&userData));
        
        co_return userData + res;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_H__