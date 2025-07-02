#ifndef __TINY_CORO_EXAMPLE_VOID_TASK_H__
#define __TINY_CORO_EXAMPLE_VOID_TASK_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_voidTask(auto& scheduler)
{
    SyncOut() << "\n\nExample_voidTask:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    tinycoro::AllOf(scheduler, task());

    SyncOut() << "co_return => void" << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_VOID_TASK_H__