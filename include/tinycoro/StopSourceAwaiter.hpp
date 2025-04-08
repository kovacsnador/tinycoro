#ifndef TINY_CORO_STOP_SOURCE_AWAITER_HPP
#define TINY_CORO_STOP_SOURCE_AWAITER_HPP

#include <stop_token>

#include "Exception.hpp"

namespace tinycoro
{
    template<typename StopSourceT = std::stop_source>
    struct StopSourceAwaiter
    {
        StopSourceT* stopSource;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr auto await_suspend(auto parentCoro)
        { 
            stopSource = std::addressof(parentCoro.promise().stopSource);

            if(stopSource->stop_possible() == false)
            {
                throw StopSourceAwaiterException{"No stop state. Need AnyOf context"};
            }

            return parentCoro;
        }

        constexpr auto await_resume() const noexcept {
            return *stopSource;
        }
    };

    template<typename StopSourceT = std::stop_source>
    struct StopTokenAwaiter
    {
        StopSourceT* stopSource;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        constexpr auto await_suspend(auto parentCoro)
        { 
            stopSource = std::addressof(parentCoro.promise().stopSource);

            if(stopSource->stop_possible() == false)
            {
                throw StopSourceAwaiterException{"No stop state. Need AnyOf context"};
            }

            return parentCoro;
        }

        constexpr auto await_resume() const noexcept {
            return stopSource->get_token();
        }
    };
    
} // namespace tinycoro


#endif // TINY_CORO_STOP_SOURCE_AWAITER_HPP