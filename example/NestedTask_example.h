#ifndef __TINY_CORO_EXAMPLE_NESTED_TASK_H__
#define __TINY_CORO_EXAMPLE_NESTED_TASK_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_nestedTask(auto& scheduler)
{
    SyncOut() << "\n\nExample_nestedTask:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            SyncOut() << "    Nested Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
            co_return 42;
        };

        auto val = co_await nestedTask();

        co_return val;
    };

    auto future = scheduler.Enqueue(task());

    SyncOut() << "co_return => " << future.get() << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_NESTED_TASK_H__