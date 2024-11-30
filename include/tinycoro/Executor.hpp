#ifndef __TINY_CORO_EXECUTOR_HPP__
#define __TINY_CORO_EXECUTOR_HPP__

#include "Common.hpp"

namespace tinycoro
{
    namespace concepts
    {
        template<typename T>
        concept LocalRunable = requires(T t)
        {
            {t.await_resume()};
            {t.GetPauseHandler()};
            {t.SetPauseHandler([]{})};
            {t.Resume()} -> std::same_as<ETaskResumeState>;
        };
    }

    template<concepts::LocalRunable TaskT>
    [[nodiscard]] auto RunOnThisThread(TaskT&& task)
    {
        using enum ETaskResumeState;

        auto pauseResumerCallback = [&task] {
            auto pauseHandler = task.GetPauseHandler();
            pauseHandler->Resume();
        };

        auto pauseHandler = task.SetPauseHandler(pauseResumerCallback);

        ETaskResumeState state{DONE};

        do
        {
            state = task.Resume();

            if(state == PAUSED)
            {
                // wait for resumption
                pauseHandler->AtomicWait(true);
            }

        } while (state != DONE && state != STOPPED);

        return task.await_resume();
    }

    template<typename... TaskT>
        requires (sizeof...(TaskT) > 1)
    [[nodiscard]] auto RunOnThisThread(TaskT&&... tasks)
    {
        auto returnValueConverter = []<typename T>(T&& task)
        {
            if constexpr (requires { {RunOnThisThread(std::forward<T>(task))} -> std::same_as<void>; })
            {
                RunOnThisThread(std::forward<T>(task));
                return VoidType{};
            }
            else
            {
                return RunOnThisThread(std::forward<T>(task));
            }
        };

        return std::make_tuple(returnValueConverter(std::forward<TaskT>(tasks))...);
    }
    
} // namespace tinycoro

#endif //!__TINY_CORO_EXECUTOR_HPP__