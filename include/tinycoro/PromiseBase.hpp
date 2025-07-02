// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_PROMISE_BASE_HPP
#define TINY_CORO_PROMISE_BASE_HPP

#include <array>
#include <concepts>
#include <future>
#include <new>

#include "Common.hpp"
#include "IntrusivePtr.hpp"
#include "UnsafeFunction.hpp"
#include "LinkedUtils.hpp"

#include "SimpleStorage.hpp"

namespace tinycoro { namespace detail {

    // This is the base class of the promise type.
    // It is necessary to support different return types in promise types.
    //
    // IMPORTANT: This base class must be the first base class
    // from which the derived promise type inherits.
    // The reason is due to alignment within the coroutine frame.
    // The coroutine promise is laid out in memory according to the
    // most derived type, but if we have a base class like this,
    // it is only safe to use SchedulablePromise if it appears first
    // in the inheritance list.
    //
    // So with this base class we can convert any derived promise
    // to std::coroutine_handle<SchedulablePromise>.
    // It is used inside the scheduler logic.
    template <concepts::IsAwaiter FinalAwaiterT, concepts::PauseHandler PauseHandlerT, typename StopSourceT>
    struct PromiseBase
    {
        using OnFinishCallback_t = void (*)(void*, void*);
        using PromiseBase_t = PromiseBase;

        PromiseBase() = default;

        ~PromiseBase()
        {
            if (parent == nullptr)
            {
                // The parent coroutine is nullptr,
                // that means this is a root
                // coroutine promise.
                //
                // Only trigger stop, if this
                // is a root coroutine
                stopSource.request_stop();
            }

            if (_destroyNotifier)
            {
                // notify others that the task
                // is destroyed.
                _destroyNotifier();
            }
        }

        // Disallow copy and move
        PromiseBase(PromiseBase&&) = delete;

        // These are navigation pointers used to
        // resume and chain together coroutines,
        // enabling continuous execution.
        PromiseBase_t* parent{nullptr};
        PromiseBase_t* child{nullptr};

        // At the beginning we not initialize
        // the stop source here, the initialization
        // will be delayed, until we actually need this object
        StopSourceT stopSource{std::nostopstate};

        // This is the shared pause handler.
        // It's shared between parent and child promises.
        //
        // Todo: consider to rename it to sharedState or so...
        detail::IntrusivePtr<PauseHandlerT> pauseHandler;

        // Gets back the pause state.
        [[nodiscard]] auto& PauseState() noexcept { return pauseHandler->pauseState; }

        // Creates the pause handler shared object
        template <typename... Args>
        auto MakePauseHandler(Args&&... args)
        {
            pauseHandler.emplace(std::forward<Args>(args)...);
            return pauseHandler.get();
        }

        // Sets a stop state.
        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            stopSource = std::forward<T>(arg);
        }

        // Getting the corresponding stop source,
        // and make sure it is initialized
        auto& StopSource() noexcept
        {
            if (stopSource.stop_possible() == false)
            {
                // initialize stop source
                // if it has no state yet
                stopSource = {};
            }
            return stopSource;
        }

        // Sets the destroyer notifier callback
        //
        // It is used in the "AllOfAwait" like context.
        template <std::regular_invocable T>
        void SetDestroyNotifier(T&& cb) noexcept
        {
            _destroyNotifier = std::forward<T>(cb);
        }

        [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

        [[nodiscard]] FinalAwaiterT final_suspend() const noexcept { return {}; }

        constexpr void unhandled_exception() const { std::rethrow_exception(std::current_exception()); }

    private:
        // callback to notify others if
        // the coroutine is destroyed.
        // std::function<void()> destroyNotifier;
        detail::UnsafeFunction<void(void*)> _destroyNotifier;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_PROMISE_BASE_HPP