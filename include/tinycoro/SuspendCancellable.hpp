#ifndef __TINY_CORO_CANCELLABLE_AWAITER_HPP__
#define __TINY_CORO_CANCELLABLE_AWAITER_HPP__

#include <coroutine>
#include <stop_token>

namespace tinycoro {

    template <typename StopTokenT = std::stop_token>
    struct SuspendCancellable
    {
        StopTokenT* stopToken;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }
        constexpr void await_suspend(auto parentCoro) noexcept { stopToken = std::addressof(parentCoro.promise().stopToken); }
        [[nodiscard]] constexpr bool await_resume() const noexcept { return stopToken->stop_requested(); }
    };
} // namespace tinycoro

#endif //!__TINY_CORO_CANCELLABLE_AWAITER_HPP__
