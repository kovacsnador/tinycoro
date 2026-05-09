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
        template <typename PromiseT>
        static inline void Resume(PromiseT& promise)
        {
            auto*        sharedState = promise.SharedState();
            const auto& stopSource   = promise.stopSource;

            assert(sharedState);
            assert(sharedState->conti);

            // reset the pause state by every resume.
            sharedState->ClearPauseStateBits();

            if (sharedState->IsCancellable() && stopSource.stop_requested())
            {
                return; // need to cancel the corouitne
            }

            // Resets all the flags.
            sharedState->ClearFlags();

            // check for continuation type
            using promise_base_t = PromiseT::PromiseBase_t;
            auto* promiseToResume = static_cast<promise_base_t*>(sharedState->conti);

            // Resume the coroutine.
            //
            // Note:
            // Ensure that promise_base_t has the same alignment
            // as the derived promise class.
            // Currently, we use alignas(std::max_align_t) for the base class
            // because mismatched alignment caused issues specially on 32-bit builds.
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