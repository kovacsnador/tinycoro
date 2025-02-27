#ifndef __TINY_CORO_CANCELLABLE_AWAITER_HPP__
#define __TINY_CORO_CANCELLABLE_AWAITER_HPP__

#include <coroutine>

#include "PauseHandler.hpp"

namespace tinycoro {

    struct CancellableSuspend
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr void await_suspend(auto coro) const noexcept
        { 
            context::MakeCancellable(coro);
        }

        constexpr void await_resume() const noexcept {}
    };
    
} // namespace tinycoro

#endif //!__TINY_CORO_CANCELLABLE_AWAITER_HPP__
