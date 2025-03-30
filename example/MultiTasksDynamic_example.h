#ifndef __TINY_CORO_EXAMPLE_MULTI_TASKS_DYNAMIC_H__
#define __TINY_CORO_EXAMPLE_MULTI_TASKS_DYNAMIC_H__

#include <tinycoro/tinycoro_all.h>

#include <vector>

#include "Common.h"

void Example_multiTasksDynamic(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiTasksDynamic:\n";

    auto task1 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 41;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 42;
    };

    auto task3 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 43;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto results = tinycoro::GetAll(scheduler, std::move(tasks));

    SyncOut() << "GetAll co_return => " << *results[0] << ", " << *results[1] << ", " << *results[2] << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_MULTI_TASKS_DYNAMIC_H__