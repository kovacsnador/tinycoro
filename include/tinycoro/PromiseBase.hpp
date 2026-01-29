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
#include "LinkedUtils.hpp"
#include "SharedState.hpp"
#include "PtrVisitor.hpp"

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
    template <concepts::IsAwaiter FinalAwaiterT, typename StopSourceT>
    struct alignas(std::max_align_t) PromiseBase
    {
        // alignas(std::max_align_t) ensures that on 32-bit systems, 
        // PromiseBase has the same alignment as the derived PromiseT
        // for all platforms.

        using PromiseBase_t = PromiseBase;

        PromiseBase() = default;

        ~PromiseBase()
        {
            auto stopTokenUser = SharedState() ? SharedState()->IsStopTokenUser() : false;

            if (parent == nullptr && stopTokenUser == false)
            {
                // The parent coroutine is nullptr,
                // that means this is a root
                // coroutine promise.
                //
                // Only trigger stop, if this
                // is a root coroutine.
                //
                // If we have no rights (we are stopToken user)
                // we not trigger it.
                stopSource.request_stop();
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

        // Sets custom user data for the promise. This pointer is reserved
        // for storing arbitrary data associated with the coroutine.
        //
        // For example:
        //  - the current awaitable pointer for CoWait constructs, or
        //  - any data required for resumption in a TaskGroup.
        void SetCustomData(void* data) noexcept
        {   
            assert(parent == nullptr);  // need to be a root corouitne
            assert(_customData == nullptr); //  must not be set

            _customData = data;
        }

        // Get the stored custom data
        [[nodiscard]] constexpr auto CustomData() const noexcept
        {
            assert(parent == nullptr);  // need to be a root corouitne

            return _customData;
        }

        [[nodiscard]] constexpr std::suspend_always initial_suspend() const noexcept { return {}; }

        [[nodiscard]] constexpr FinalAwaiterT final_suspend() const noexcept { return {}; }

        constexpr void unhandled_exception() const { std::rethrow_exception(std::current_exception()); }

        constexpr auto SharedState() noexcept
        {
            return detail::PtrVisit(_sharedState);
        }

        constexpr auto SharedState() const noexcept
        {
            return detail::PtrVisit(_sharedState);
        }

        constexpr void AssignSharedState(auto sharedStatePtr) noexcept
        {
            assert(sharedStatePtr);

            _sharedState = sharedStatePtr;
        }

        constexpr void CreateSharedState(bool initialCancellable = tinycoro::default_initial_cancellable_policy::value) noexcept
        {
            // make sure this is called only once
            assert(std::get<0>(_sharedState) == nullptr);

            _sharedState.emplace<detail::SharedState>(initialCancellable);
        }  

    private:
        // Saving the current awaitable.
        //
        // It is used in the "AllOfAwait" and "AnyOfAwait" context (with scheduler),
        // to be able to resume the awaitable.
        void* _customData{nullptr};

        // Contains shared state information across
        // different coroutines. (e.g. parent -> child)
        //
        // Parent owns the shared state and passes forward
        // to his child coroutines.
        detail::SharedState_t _sharedState{nullptr};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_PROMISE_BASE_HPP