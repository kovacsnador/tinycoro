// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_STOP_SOURCE_AWAITER_HPP
#define TINY_CORO_STOP_SOURCE_AWAITER_HPP

#include <stop_token>

#include "Exception.hpp"

namespace tinycoro {
    template <typename StopSourceT = std::stop_source>
    struct StopSourceAwaiter
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr bool await_suspend(auto parentCoro)
        {
            auto& stopSource = parentCoro.promise().StopSource();
            assert(stopSource.stop_possible());

            _stopSource = std::addressof(stopSource);

            // no suspend
            return false;
        }

        constexpr auto await_resume() const noexcept { return *_stopSource; }

    private:
        StopSourceT* _stopSource;
    };

    template <typename StopSourceT = std::stop_source>
    struct StopTokenAwaiter
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr bool await_suspend(auto parentCoro)
        {
            auto& stopSource = parentCoro.promise().StopSource();
            assert(stopSource.stop_possible());

            _stopSource = std::addressof(stopSource);

            // no suspend
            return false;
        }

        constexpr auto await_resume() const noexcept { return _stopSource->get_token(); }

    private:
        StopSourceT* _stopSource;
    };

} // namespace tinycoro

#endif // TINY_CORO_STOP_SOURCE_AWAITER_HPP