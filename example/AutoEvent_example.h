#ifndef __TINY_CORO_EXAMPLE_AUTO_EVENT_H__
#define __TINY_CORO_EXAMPLE_AUTO_EVENT_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_AutoEvent(auto& scheduler)
{
    SyncOut() << "\n\nExample_AutoEvent:\n";

    int32_t val{};

    tinycoro::AutoEvent event;

    auto producer = [&]() -> tinycoro::Task<> {
        val = 42;
        event.Set();

        co_return;
    };

    auto consumer = [&]() -> tinycoro::Task<int32_t> {
        co_await event;
        co_return val;
    };


    auto [result1, result2] = tinycoro::AllOf(scheduler, consumer(), producer());

    assert(*result1 == 42);

    SyncOut() << "co_return => " << *result1 << '\n';
}

#endif //__TINY_CORO_EXAMPLE_AUTO_EVENT_H__