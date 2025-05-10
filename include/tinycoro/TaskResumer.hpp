#ifndef TINY_CORO_TASK_RESUMER_HPP
#define TINY_CORO_TASK_RESUMER_HPP

#include "Common.hpp"

namespace tinycoro {

    struct TaskResumer
    {
        template<typename PromiseT>
        static inline void Resume(PromiseT& promise)
        {
            auto& pauseHandler = promise.pauseHandler;
            const auto& stopSource = promise.stopSource;

            // reset the pause state by every resume.
            promise.pauseState.store(EPauseState::IDLE, std::memory_order_relaxed);

            if (pauseHandler)
            {
                if (stopSource.stop_requested() && pauseHandler->IsCancellable())
                {
                    return; // need to cancel the corouitne
                }

                // Resets the pause flag if necessary so the task is running.
                pauseHandler->Resume();
            }

            // check for child type
            using promise_base_t = std::remove_pointer_t<decltype(promise.child)>; 

            // find the last child
            promise_base_t* promisePtr = std::addressof(promise);
            while (promisePtr && promisePtr->child)
            {
                promisePtr = promisePtr->child;
            }

            // resume the coroutine
            auto handle = std::coroutine_handle<promise_base_t>::from_promise(*promisePtr);
            handle.resume();
        }

        [[nodiscard]] static inline ETaskResumeState ResumeState(auto handle) noexcept
        {
            if (handle && handle.done() == false)
            {
                auto& promise = handle.promise();

                if(promise.exception)
                {   
                    // if there was an unhandled
                    // exception the task is done
                    return ETaskResumeState::DONE; 
                }

                const auto& pauseHandler = promise.pauseHandler;
                const auto& stopSource = promise.stopSource;

                if (pauseHandler)
                {
                    if (stopSource.stop_requested() && pauseHandler->IsCancellable())
                    {
                        return ETaskResumeState::STOPPED;
                    }
                    else if (pauseHandler->IsPaused())
                    {
                        return ETaskResumeState::PAUSED;
                    }
                }
                return ETaskResumeState::SUSPENDED;
            }
            return ETaskResumeState::DONE;
        }
    };
} // namespace tinycoro

#endif // TINY_CORO_TASK_RESUMER_HPP