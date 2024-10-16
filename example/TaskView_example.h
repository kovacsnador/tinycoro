#ifndef __TINY_CORO_EXAMPLE_TASK_VIEW_H__
#define __TINY_CORO_EXAMPLE_TASK_VIEW_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_taskView(auto& scheduler)
{
    SyncOut() << "\n\nExample_taskView:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto coro   = task();
    tinycoro::GetAll(scheduler, coro.TaskView());

    SyncOut() << "co_return => void" << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_TASK_VIEW_H__