#ifndef __TINY_CORO_EXAMPLE_VOID_TASK_H__
#define __TINY_CORO_EXAMPLE_VOID_TASK_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

tinycoro::Task<void> SimpleVoidCoro()
{
    co_return;
}

void Example_voidTask()
{
    SyncOut() << "\n\nExample_voidTask:\n";

    tinycoro::AllOf(SimpleVoidCoro());
}

#endif //!__TINY_CORO_EXAMPLE_VOID_TASK_H__