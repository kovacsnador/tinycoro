#ifndef __TINY_CORO_EXAMPLE_EXCEPTION_H__
#define __TINY_CORO_EXAMPLE_EXCEPTION_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_exception(auto& scheduler)
{
    SyncOut() << "\n\nExample_exception:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        throw std::runtime_error("Example_exception exception");

        co_return;
    };

    auto future = scheduler.Enqueue(task());

    try
    {
        future.get();
    }
    catch (const std::exception& e)
    {
        SyncOut() << e.what() << '\n';
    }
}

#endif //!__TINY_CORO_EXAMPLE_EXCEPTION_H__