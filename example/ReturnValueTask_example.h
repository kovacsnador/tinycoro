#ifndef __TINY_CORO_EXAMPLE_RETURN_VALUE_H__
#define __TINY_CORO_EXAMPLE_RETURN_VALUE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_returnValueTask(auto& scheduler)
{
    SyncOut() << "\n\nExample_returnValueTask:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_return 42;
    };

    auto val = tinycoro::GetAll(scheduler, task());

    SyncOut() << "co_return => " << val << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_RETURN_VALUE_H__