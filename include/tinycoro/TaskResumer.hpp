#ifndef __TINY_CORO_TASK_RESUMER_HPP__
#define __TINY_CORO_TASK_RESUMER_HPP__

#include "Common.hpp"
#include "PackedCoroHandle.hpp"

namespace tinycoro {
    struct TaskResumer
    {
        ETaskResumeState operator()(auto coroHdl, const auto& stopSource)
        {
            if constexpr (requires { coroHdl.promise().pauseHandler; })
            {
                if (coroHdl.promise().pauseHandler)
                {
                    // Resets the pause flag if necessary so the task is running.
                    coroHdl.promise().pauseHandler->Resume();
                }
            }

            if (stopSource.stop_requested() && coroHdl.promise().cancellable)
            {
                return ETaskResumeState::STOPPED;
            }

            PackedCoroHandle  hdl{coroHdl};
            PackedCoroHandle* hdlPtr = std::addressof(hdl);

            while (*hdlPtr && hdlPtr->Child() && hdlPtr->Child().Done() == false)
            {
                hdlPtr = std::addressof(hdlPtr->Child());
            }

            if (*hdlPtr && hdlPtr->Done() == false)
            {
                return hdlPtr->Resume();
            }

            return ETaskResumeState::DONE;
        }
    };
} // namespace tinycoro

#endif //!__TINY_CORO_TASK_RESUMER_HPP__