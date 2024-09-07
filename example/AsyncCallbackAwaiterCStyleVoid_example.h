#ifndef __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_VOID_H__
#define __TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_VOID_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_asyncCallbackAwaiter_CStyleVoid(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_CStyle2:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        
        auto task2 = []() -> tinycoro::Task<void> {
                SyncOut() << "  Task2 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';


            auto cb = [](void* userData, int i) {
                SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';

                auto null = tinycoro::UserData::Get<std::nullptr_t>(userData);
                assert(null == nullptr);
            };

            std::nullptr_t nullp;

            co_await tinycoro::AsyncCallbackAwaiter_CStyle([](auto cb, auto userData) { AsyncCallbackAPIvoid(cb, userData); }, cb, tinycoro::IndexedUserData<0>(nullp));
        };

        SyncOut() << "  Task1 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());

    future.get();

    SyncOut() << "co_return => void" << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_ASYNC_CALLBACK_AWAITER_CSTYLE_VOID_H__