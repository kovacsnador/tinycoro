// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CANCELLABLE_AWAITER_HPP
#define TINY_CORO_CANCELLABLE_AWAITER_HPP

#include <coroutine>

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

    namespace this_coro
    {   
        // This yield is not cancellable
        //
        // If you need a yield which is cancellable
        // use yield_cancellable().
        constexpr auto yield() noexcept -> std::suspend_always
        {
            return {};
        }

        // A cancellable yield.
        constexpr auto yield_cancellable() noexcept -> tinycoro::CancellableSuspend
        {
            return {};
        }
    }
    
} // namespace tinycoro

#endif //TINY_CORO_CANCELLABLE_AWAITER_HPP
