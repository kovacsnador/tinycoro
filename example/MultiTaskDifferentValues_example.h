#ifndef __TINY_CORO_EXAMPLE_MULTI_TASK_DIFFERENT_VALUES_H__
#define __TINY_CORO_EXAMPLE_MULTI_TASK_DIFFERENT_VALUES_H__

#include <tinycoro/tinycoro_all.h>

#include <vector>

#include "Common.h"

void Example_multiTaskDifferentValues(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiTaskDifferentValues:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        throw std::runtime_error("Exception throwed!");

        co_return 42;
    };

    struct S
    {
        int32_t i;
    };

    auto task3 = []() -> tinycoro::Task<S> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 43;
    };

    auto futures = scheduler.EnqueueTasks(task1(), task2(), task3());

    try
    {
        auto results = tinycoro::GetAll(futures);

        auto voidType = std::get<0>(results);
        auto val      = std::get<1>(results);
        auto s        = std::get<2>(results);

        SyncOut() << std::boolalpha << "GetAll task1 co_return => void " << std::is_same_v<tinycoro::VoidType, std::decay_t<decltype(voidType)>>
                  << '\n';
        SyncOut() << "GetAll task2 co_return => " << val << '\n';
        SyncOut() << "GetAll task3 co_return => " << s.i << '\n';
    }
    catch (const std::exception& e)
    {
        SyncOut() << e.what() << '\n';
    }
}

#endif //!__TINY_CORO_EXAMPLE_MULTI_TASK_DIFFERENT_VALUES_H__