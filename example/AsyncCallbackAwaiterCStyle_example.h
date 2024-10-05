#ifndef __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_H__
#define __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_asyncCallbackAwaiter_CStyle(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_CStyle:\n";

    auto task = []() -> tinycoro::Task<int32_t> {

        SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        auto cb = [](tinycoro::UserData userData, int i) {
            SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';

            auto& d = userData.Get<int>(); 
            d += 21;
        };
        
        int userData{21};
        co_await tinycoro::MakeAsyncCallbackAwaiter_CStyle(AsyncCallbackAPIvoid, tinycoro::UserCallback{cb}, tinycoro::UserData{&userData});
        
        co_return userData;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_H__