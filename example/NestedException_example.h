#ifndef __TINY_CORO_EXAMPLE_NESTED_EXCEPTION_H__
#define __TINY_CORO_EXAMPLE_NESTED_EXCEPTION_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_nestedException(auto& scheduler)
{
    SyncOut() << "\n\nExample_nestedException:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            SyncOut() << "    Nested Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

            throw std::runtime_error("Example_nestedException nested exception");

            co_return 42;
        };

        auto val = co_await nestedTask();

        co_return val;
    };

    auto future = scheduler.Enqueue(task());

    try
    {
        auto val = future.get();
        SyncOut() << "co_return => " << val.value() << '\n';
    }
    catch (const std::exception& e)
    {
        SyncOut() << e.what() << '\n';
    }
}

#endif //!__TINY_CORO_EXAMPLE_NESTED_EXCEPTION_H__