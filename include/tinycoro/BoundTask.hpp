#ifndef __TINY_CORO_BOUND_TASK_HPP__
#define __TINY_CORO_BOUND_TASK_HPP__

#include <memory>

namespace tinycoro {

    // Binds the coroutine function and the Task together to manage the
    // lifecycle of both object as one.
    // So if the coroutine function is a lambda object (with capture list),
    // his lifetime is extended until the Task is done.
    template <typename CoroutineFunctionT, typename TaskT>
    struct BoundTask : private TaskT
    {
        using promise_type  = TaskT::promise_type;
        using value_type = promise_type::value_type;

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

        using TaskT::IsDone;

        using TaskT::SetStopSource;
        using TaskT::SetDestroyNotifier;

        using TaskT::Address;

    private:
        CoroutineFunctionT _function;
    };

    template<typename CoroutineFunctionT, typename... Args>
    [[nodiscard]] auto MakeBound(CoroutineFunctionT&& func, Args&&... args)
    {
        auto functionPtr = std::make_unique<std::decay_t<CoroutineFunctionT>>(std::forward<CoroutineFunctionT>(func));
        auto task = (*functionPtr)(std::forward<Args>(args)...);

        return BoundTask<decltype(functionPtr), decltype(task)>{std::move(functionPtr), std::move(task)};
    }

} // namespace tinycoro

#endif //!__TINY_CORO_BOUND_TASK_HPP__