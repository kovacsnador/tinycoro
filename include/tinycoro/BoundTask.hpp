#ifndef __TINY_CORO_BOUND_TASK_HPP__
#define __TINY_CORO_BOUND_TASK_HPP__

namespace tinycoro {

    template <typename CoroutineFunctionT, typename TaskT>
    struct BoundTask
    {
        explicit BoundTask(CoroutineFunctionT function, TaskT task)
        : _function{std::move(function)}
        , _task{std::move(task)}
        {
        }

        void Resume() { _task.Resume(); }

        [[nodiscard]] auto ResumeState() { return _task.ResumeState(); }

        auto SetPauseHandler(auto pauseResume)
        {
            return _task.SetPauseHandler(std::move(pauseResume));
        }

        bool IsPaused() const noexcept
        {
            return _task.IsPaused();
        }

        auto GetPauseHandler() noexcept { return _task.GetPauseHandler(); }

        [[nodiscard]] auto TaskView() const noexcept {  return _task.TaskView(); }

        template<typename T>
        void SetStopSource(T&& arg)
        {
            _task.SetStopSource(std::forward<T>(arg));
        }

        template <typename T>
        void SetDestroyNotifier(T&& cb)
        {
            _task.SetDestroyNotifier(std::forward<T>(cb));
        }

    private:
        CoroutineFunctionT _function;
        TaskT              _task;
    };

    template<typename CoroutineFunctionT, typename... Args>
    auto MakeBound(CoroutineFunctionT&& func, Args&&... args)
    {
        // creating the coroutine task
        auto task = func(std::forward<Args>(args)...);

        // Put it all together in a task wrapper to manage life cycle of the possible lambda object as func.
        if constexpr(std::copy_constructible<CoroutineFunctionT>)
        {
            // if we can we will copy the function.
            return BoundTask{func, std::move(task)};
        }
        else
        {
            // If the function or lambda is not copyable, we move it.
            return BoundTask{std::move(func), std::move(task)};
        }
    }

} // namespace tinycoro

#endif //!__TINY_CORO_BOUND_TASK_HPP__