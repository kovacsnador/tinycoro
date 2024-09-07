#ifndef __TINY_CORO_EXAMPLE_GENERATOR_H__
#define __TINY_CORO_EXAMPLE_GENERATOR_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_generator()
{
    SyncOut() << "\n\nExample_generator:\n";

    struct S
    {
        int32_t i;
    };

    auto generator = [](int32_t max) -> tinycoro::Generator<S> {
        SyncOut() << "  Coro generator..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        for (auto it : std::views::iota(0, max))
        {
            SyncOut() << "  Yield value: " << it << "  Thread id : " << std::this_thread::get_id() << '\n';
            co_yield S{it};
        }
    };

    for (const auto& it : generator(12))
    {
        SyncOut() << "Generator Value: " << it.i << '\n';
    }
}

#endif //!__TINY_CORO_EXAMPLE_GENERATOR_H__