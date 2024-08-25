#ifndef __TINY_CORO_CANCELLABLE_AWAITER_HPP__
#define __TINY_CORO_CANCELLABLE_AWAITER_HPP__

#include <coroutine>
#include <stop_token>

namespace tinycoro {

    template <typename ReturnT>
    struct CancellableSuspend
    {
        CancellableSuspend(ReturnT& returnValue)
        : _returnValue{returnValue}
        {
        }

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr void await_suspend(auto parentCoro) const noexcept
        { 
            parentCoro.promise().cancellable = true;
            parentCoro.promise().return_value(std::move(_returnValue));
        }

        constexpr void await_resume() const noexcept { }

    private:
        ReturnT& _returnValue;
    };

    template<>
    struct CancellableSuspend<void>
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr void await_suspend(auto parentCoro) const noexcept
        { 
            parentCoro.promise().cancellable = true;
        }

        constexpr void await_resume() const noexcept { }
    };
} // namespace tinycoro

#endif //!__TINY_CORO_CANCELLABLE_AWAITER_HPP__
