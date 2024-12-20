#ifndef __TINY_CORO_BOUND_TASK_HPP__
#define __TINY_CORO_BOUND_TASK_HPP__

namespace tinycoro {

    // Binds the coroutine function and the Task together to manage the
    // lifecycle of both object as one.
    // So if the coroutine function is a lambda object (with capture list),
    // his lifetime is extended until the Task is done.
    template <typename CoroutineFunctionT, typename TaskT>
    struct BoundTask : private TaskT 
    {
        using promise_type  = TaskT::promise_type;

        explicit BoundTask(CoroutineFunctionT function, TaskT task)
        : TaskT{std::move(task)}
        , _function{std::move(function)}
        {
        }

        using TaskT::await_ready;
        using TaskT::await_resume;
        using TaskT::await_suspend;

        using TaskT::Resume;
        using TaskT::ResumeState;

        using TaskT::SetPauseHandler;
        using TaskT::GetPauseHandler;

        using TaskT::IsPaused;
        using TaskT::TaskView;
        
        using TaskT::SetStopSource;
        using TaskT::SetDestroyNotifier;

    private:
        CoroutineFunctionT _function;
    };

    template<typename CoroutineFunctionT, typename... Args>
    [[nodiscard]] auto MakeBound(CoroutineFunctionT&& func, Args&&... args)
    {
        // creating the coroutine task
        auto task = func(std::forward<Args>(args)...);

        // binding the corouitne function and the task together.
        return BoundTask<std::decay_t<CoroutineFunctionT>, decltype(task)>{std::forward<CoroutineFunctionT>(func), std::move(task)};
    }

} // namespace tinycoro

#endif //!__TINY_CORO_BOUND_TASK_HPP__