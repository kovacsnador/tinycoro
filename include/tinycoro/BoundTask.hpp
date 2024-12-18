#ifndef __TINY_CORO_BOUND_TASK_HPP__
#define __TINY_CORO_BOUND_TASK_HPP__

namespace tinycoro {

    // Binds the coroutine function and the Task together to manage the
    // lifecycle of both object as one.
    // So if the coroutine function is a lambda object (with capture list),
    // his lifetime is extended until the Task is done.
    template <typename CoroutineFunctionT, typename TaskT>
    struct BoundTask
    {
        using promise_type  = TaskT::promise_type;

        explicit BoundTask(CoroutineFunctionT function, TaskT task)
        : _function{std::move(function)}
        , _task{std::move(task)}
        {
        }

        auto await_ready()
        {
            return _task.await_ready();
        }

        [[nodiscard]] auto await_resume()
        {
            return _task.await_resume();
        }

        auto await_suspend(auto hdl)
        {
            return _task.await_suspend(hdl);
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
    [[nodiscard]] auto MakeBound(CoroutineFunctionT&& func, Args&&... args)
    {
        // creating the coroutine task
        auto task = func(std::forward<Args>(args)...);

        // binding the corouitne function and the task together.
        return BoundTask{std::forward<CoroutineFunctionT>(func), std::move(task)};
    }

} // namespace tinycoro

#endif //!__TINY_CORO_BOUND_TASK_HPP__