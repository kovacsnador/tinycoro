#ifndef __TINY_CORO_EXAMPLE_AGGREGATE_VALUE_H__
#define __TINY_CORO_EXAMPLE_AGGREGATE_VALUE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void Example_aggregateValue(auto& scheduler)
{
    SyncOut() << "\n\nExample_aggregateValue:\n";

    struct Aggregate
    {
        int32_t i;
        int32_t j;
    };

    auto task = []() -> tinycoro::Task<Aggregate> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_return Aggregate{42, 43};
    };

    auto val = tinycoro::GetAll(scheduler, task());

    SyncOut() << "co_return => " << val->i << " " << val->j << '\n';
}


#endif //!__TINY_CORO_EXAMPLE_AGGREGATE_VALUE_H__