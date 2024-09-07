#ifndef __TINY_CORO_EXAMPLE_MULTI_TASKS_H__
#define __TINY_CORO_EXAMPLE_MULTI_TASKS_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_multiTasks(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiTasks:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto futures = scheduler.EnqueueTasks(task(), task(), task());
    tinycoro::GetAll(futures);

    SyncOut() << "GetAll co_return => void" << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_MULTI_TASKS_H__