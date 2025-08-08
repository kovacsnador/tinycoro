#ifndef TINY_CORO_CALL_ONCE_HPP
#define TINY_CORO_CALL_ONCE_HPP

#include <concepts>
#include <functional>
#include <atomic>

namespace tinycoro { namespace detail {

    // Calls the provided function exactly once across all threads that call this function.
    // Uses an atomic flag to ensure the function is only invoked once.
    //
    // Parameters:
    //   - flag: An atomic flag (must support test_and_set(memory_order)) shared across threads.
    //   - memoryOrder: The memory ordering to use for the test_and_set operation.
    //                  Should be at least std::memory_order_acquire; use std::memory_order_acq_rel
    //                  if this thread is expected to perform initialization.
    //   - func: The callable object to be invoked once.
    //   - args: Arguments to be forwarded to the callable.
    //
    // Returns:
    //   - true if this thread successfully executed the callable (i.e., was the first to set the flag).
    //   - false if another thread had already set the flag and invoked the callable.
    template <typename FlagT, typename FuncT, typename... Args>
        requires std::invocable<FuncT, Args...>
    constexpr bool CallOnce(FlagT& flag, std::memory_order memoryOrder, FuncT&& func, Args&&... args)
    {
        // Atomically test and set the flag using the provided memory order.
        // If the flag was not previously set (i.e., returns false), invoke the callable.
        if (flag.test_and_set(memoryOrder) == false)
        {
            // First thread to set the flag: invoke the function with forwarded arguments.
            std::invoke(std::forward<FuncT>(func), std::forward<Args>(args)...);
            return true;
        }

        // Another thread has already set the flag; do nothing.
        return false;
    }

}} // namespace tinycoro::detail

#endif // TINY_CORO_CALL_ONCE_HPP