// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CANCELLABLE_AWAITER_HPP
#define TINY_CORO_CANCELLABLE_AWAITER_HPP

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

#endif //TINY_CORO_CANCELLABLE_AWAITER_HPP
