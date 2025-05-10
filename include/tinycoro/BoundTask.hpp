#ifndef TINY_CORO_BOUND_TASK_HPP
#define TINY_CORO_BOUND_TASK_HPP

#include <memory>
#include <type_traits>

#include "AnyObject.hpp"

namespace tinycoro {

    // Binds the coroutine function and the Task together to manage the
    // lifecycle of both object as one.
    // So if the coroutine function is a lambda object (with capture list),
    // his lifetime is extended until the Task is done.
    template <typename TaskT>
    struct BoundTask : private TaskT
    {
        using promise_type = typename TaskT::promise_type;
        using value_type   = typename promise_type::value_type;

        template<typename CoroutineFunctionT>
        explicit BoundTask(CoroutineFunctionT function, TaskT task)
        : TaskT{std::move(task)}
        , _anyFunction{std::move(function)}
        {
        }

        using TaskT::await_ready;
        using TaskT::await_resume;
        using TaskT::await_suspend;

        using TaskT::Resume;
        using TaskT::ResumeState;

        using TaskT::GetPauseHandler;
        using TaskT::SetPauseHandler;

        using TaskT::IsDone;

        using TaskT::SetDestroyNotifier;
        using TaskT::SetStopSource;

        using TaskT::Address;
        using TaskT::Release;

        detail::AnyObject&& MoveAnyFunction() noexcept { return std::move(_anyFunction); }

    private:
        // the underlying corouitne function,
        // which is allocated on the heap
        // in order to be able to extend his life time.
        detail::AnyObject _anyFunction;
    };

    // Helper function to create
    // BoundTask, (Corouitne function + Task)
    template <typename CoroutineFunctionT, typename... Args>
    [[nodiscard]] auto MakeBound(CoroutineFunctionT&& func, Args&&... args)
    {
        auto functionPtr = std::make_unique<std::decay_t<CoroutineFunctionT>>(std::forward<CoroutineFunctionT>(func));
        auto task        = (*functionPtr)(std::forward<Args>(args)...);

        return BoundTask<decltype(task)>{std::move(functionPtr), std::move(task)};
    }

} // namespace tinycoro

#endif // TINY_CORO_BOUND_TASK_HPP