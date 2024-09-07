#ifndef __TINY_CORO_EXAMPLE_MULTI_MOVED_TASKS_DYNAMIC_VOID_H__
#define __TINY_CORO_EXAMPLE_MULTI_MOVED_TASKS_DYNAMIC_VOID_H__

#include <tinycoro/tinycoro_all.h>

#include <vector>

#include "Common.h"

void Example_multiMovedTasksDynamicVoid(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiMovedTasksDynamic:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto task2 = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto task3 = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto futures = scheduler.EnqueueTasks(std::move(tasks));
    tinycoro::GetAll(futures);
}

#endif //!__TINY_CORO_EXAMPLE_MULTI_MOVED_TASKS_DYNAMIC_VOID_H__