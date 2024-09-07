#ifndef __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_RETURN_VALUE_H__
#define __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_RETURN_VALUE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_asyncCallbackAwaiterWithReturnValue(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiterWithReturnValue:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        struct S
        {
            int i{42};
        };

        auto cb = [](void* userData, int i, int j) {
            SyncOut() << "  Callback called... " << i << " " << j << " Thread id: " << std::this_thread::get_id() << '\n';

            auto* s = tinycoro::UserData::Get<S>(userData);
            s->i++;

            // do some work
            std::this_thread::sleep_for(100ms);
        };

        S s;

        auto asyncCb = [](auto cb, auto userData){ return AsyncCallbackAPI(userData, cb); };

        // wait with return value
        auto jthread = co_await tinycoro::AsyncCallbackAwaiter_CStyle(asyncCb, cb, tinycoro::IndexedUserData<0>(&s));
        co_return s.i;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_RETURN_VALUE_H__