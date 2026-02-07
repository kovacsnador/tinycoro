// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_TASK_RESUMER_HPP
#define TINY_CORO_TASK_RESUMER_HPP

#include <cassert>

#include "Common.hpp"

namespace tinycoro { namespace detail {

    struct TaskResumer
    {
        // Find the last continuation
        // and set up the loop at the end
        // if necessary.
        template <typename PromiseBaseT>
        [[nodiscard]] static inline auto FindContinuation(PromiseBaseT* promisePtr) noexcept
        {
            assert(promisePtr);

            // Iterate until we found the last corouinte
            // in the chain, which we need to resume.
            while (promisePtr->child != nullptr)
            {
                // moving forward...
                promisePtr = promisePtr->child;
            }

            return promisePtr;
        }

        template <typename PromiseT>
        static inline void Resume(PromiseT& promise)
        {
            auto        sharedStatePtr = promise.SharedState();
            const auto& stopSource     = promise.stopSource;

            // reset the pause state by every resume.
            sharedStatePtr->ClearPauseStateBits();

            if (sharedStatePtr->IsCancellable() && stopSource.stop_requested())
            {
                return; // need to cancel the corouitne
            }

            // Resets all the flags.
            sharedStatePtr->ClearFlags();

            // check for continuation type
            using promise_base_t = std::remove_pointer_t<decltype(promise.child)>;

            // find the continuation
            auto promiseToResume = FindContinuation<promise_base_t>(std::addressof(promise));

            // Resume the coroutine.
            //
            // Note:
            // Ensure that promise_base_t has the same alignment
            // as the derived promise class.
            // Currently, we use alignas(std::max_align_t) for the base class
            // because mismatched alignment caused issues on 32-bit builds.
            auto handle = std::coroutine_handle<promise_base_t>::from_promise(*promiseToResume);
            handle.resume();
        }

        [[nodiscard]] static inline ETaskResumeState ResumeState(auto handle) noexcept
        {
            if (handle && handle.done() == false)
            {
                auto& promise = handle.promise();

                if constexpr (requires { { promise.HasException() } -> std::same_as<bool>; })
                {
                    if (promise.HasException())
                    {
                        // if there was an unhandled
                        // exception, the task is done
                        return ETaskResumeState::DONE;
                    }
                }

                const auto  sharedStatePtr = promise.SharedState();
                const auto& stopSource     = promise.stopSource;

                if (sharedStatePtr->IsCancellable() && stopSource.stop_requested())
                {
                    return ETaskResumeState::STOPPED;
                }
                else if (sharedStatePtr->IsPaused())
                {
                    return ETaskResumeState::PAUSED;
                }
                return ETaskResumeState::SUSPENDED;
            }
            return ETaskResumeState::DONE;
        }
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_TASK_RESUMER_HPP