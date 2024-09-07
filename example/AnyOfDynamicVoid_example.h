#ifndef __TINY_CORO_EXAMPLE_ANY_OF_DYNAMIC_VOID_H__
#define __TINY_CORO_EXAMPLE_ANY_OF_DYNAMIC_VOID_H__

#include <tinycoro/tinycoro_all.h>

#include <vector>

#include "Common.h"

void Example_AnyOfDynamicVoid(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOfDynamicVoid:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting... before StopTokenAwaiter " << "  Thread id : " << std::this_thread::get_id() << '\n';

        [[maybe_unused]] auto stopToken  = co_await tinycoro::StopTokenAwaiter{};
        [[maybe_unused]] auto stopSource = co_await tinycoro::StopSourceAwaiter{};

        SyncOut() << "  Coro starting... after StopTokenAwaiter " << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_await tinycoro::Sleep(duration);

        SyncOut() << std::boolalpha <<  "  Coro stop was requested:  " << stopToken.stop_requested() << "  Thread id : " << std::this_thread::get_id() << '\n';
    };

    std::stop_source source;

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task1(10ms));
    tasks.push_back(task1(20ms));
    tasks.push_back(task1(30ms));

    tinycoro::AnyOfWithStopSource(scheduler, source, tasks);

    SyncOut() << "co_return => void\n";
}

#endif //!__TINY_CORO_EXAMPLE_ANY_OF_DYNAMIC_VOID_H__