#ifndef __TINY_CORO_TASK_RESUMER_HPP__
#define __TINY_CORO_TASK_RESUMER_HPP__

#include "Common.hpp"
#include "PackedCoroHandle.hpp"

namespace tinycoro {

    struct TaskResumer
    {
        void operator()(auto coroHdl, const auto& stopSource) const
        {
            auto& pauseHandler = coroHdl.promise().pauseHandler;

            if(pauseHandler)
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

            while (*hdlPtr && hdlPtr->Child() /*&& hdlPtr->Child().Done() == false*/)
            {
                hdlPtr = std::addressof(hdlPtr->Child());
            }

            //if (*hdlPtr && hdlPtr->Done() == false)
            {
                hdlPtr->Resume();
            }
        }

        ETaskResumeState ResumeState(auto coroHdl, const auto& stopSource) const
        {
            if (coroHdl && coroHdl.done() == false)
            {
                auto& pauseHandler = coroHdl.promise().pauseHandler;

                if(pauseHandler)
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

#endif //!__TINY_CORO_TASK_RESUMER_HPP__