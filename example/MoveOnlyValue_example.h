#ifndef __TINY_CORO_EXAMPLE_MOVE_ONLY_VALUE_H__
#define __TINY_CORO_EXAMPLE_MOVE_ONLY_VALUE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_moveOnlyValue(auto& scheduler)
{
    SyncOut() << "\n\nExample_moveOnlyValue:\n";

    struct OnlyMoveable
    {
        OnlyMoveable(int32_t ii)
        : i{ii}
        {
        }

        OnlyMoveable(OnlyMoveable&& other)            = default;
        OnlyMoveable& operator=(OnlyMoveable&& other) = default;

        int32_t i;
    };

    auto task = []() -> tinycoro::Task<OnlyMoveable> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_return 42;
    };

    auto val = tinycoro::GetAll(scheduler, task());

    SyncOut() << "co_return => " << val->i << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_MOVE_ONLY_VALUE_H__