#ifndef __TINY_CORO_EXAMPLE_RETURN_VALUE_H__
#define __TINY_CORO_EXAMPLE_RETURN_VALUE_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

tinycoro::Task<int32_t> ReturnValueCoro()
{
    co_return 42;
}

void Example_returnValueTask()
{
    SyncOut() << "\n\nExample_returnValueTask:\n";

    auto val = tinycoro::AllOf(ReturnValueCoro());

    SyncOut() << "co_return => " << *val << '\n';
}

#endif //!__TINY_CORO_EXAMPLE_RETURN_VALUE_H__