#ifndef __TINY_CORO_EXAMPLE_SYNC_AWAIT_H__
#define __TINY_CORO_EXAMPLE_SYNC_AWAIT_H__

#include <string>

#include <tinycoro/tinycoro_all.h>

tinycoro::Task<std::string> Example_SyncAwait(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<std::string> { co_return "123"; };
    auto task2 = []() -> tinycoro::Task<std::string> { co_return "456"; };
    auto task3 = []() -> tinycoro::Task<std::string> { co_return "789"; };

    // waiting to finish all other tasks. (non blocking)
    auto tupleResult = co_await tinycoro::SyncAwait(scheduler, task1(), task2(), task3());

    // tuple accumulate
    co_return std::apply(
        []<typename... Ts>(Ts&&... ts) {
            std::string result;
            (result.append(std::forward<Ts>(ts)), ...);
            return result;
        },
        tupleResult);
}

#endif //!__TINY_CORO_EXAMPLE_SYNC_AWAIT_H__