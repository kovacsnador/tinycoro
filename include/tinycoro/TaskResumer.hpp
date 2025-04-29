#ifndef TINY_CORO_TASK_RESUMER_HPP
#define TINY_CORO_TASK_RESUMER_HPP

#include "Common.hpp"
#include "PackedCoroHandle.hpp"

namespace tinycoro {

    struct TaskResumer
    {
        static inline void Resume(auto coroHdl)
        {
            auto& pauseHandler = coroHdl.promise().pauseHandler;
            const auto& stopSource = coroHdl.promise().stopSource;

            if (pauseHandler)
            {
                if (stopSource.stop_requested() && pauseHandler->IsCancellable())
                {
                    return; // need to cancel the corouitne
                }

                // Resets the pause flag if necessary so the task is running.
                pauseHandler->Resume();
            }

            PackedCoroHandle  hdl{coroHdl};
            PackedCoroHandle* hdlPtr = std::addressof(hdl);

            while (*hdlPtr && hdlPtr->Child())
            {
                hdlPtr = std::addressof(hdlPtr->Child());
            }

            hdlPtr->Resume();
        }

        [[nodiscard]] static inline ETaskResumeState ResumeState(auto coroHdl) noexcept
        {
            if (coroHdl && coroHdl.done() == false)
            {
                auto& pauseHandler = coroHdl.promise().pauseHandler;
                const auto& stopSource = coroHdl.promise().stopSource;

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