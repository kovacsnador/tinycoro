#ifndef TINY_CORO_BOUND_TASK_HPP
#define TINY_CORO_BOUND_TASK_HPP

#include <memory>
#include <type_traits>

namespace tinycoro {

    // Helper function to create
    // a tinycoro task which stores the
    // underlying coroutine function object.
    template <typename CoroutineFunctionT, typename... Args>
    [[nodiscard]] auto MakeBound(CoroutineFunctionT&& func, Args&&... args)
    {
        auto functionPtr = std::make_unique<std::decay_t<CoroutineFunctionT>>(std::forward<CoroutineFunctionT>(func));
        auto task        = (*functionPtr)(std::forward<Args>(args)...);

        // moving the function pointer into the coroutine task
        task.SaveCoroutineFunction(std::move(functionPtr));
        return std::move(task);
    }

} // namespace tinycoro

#endif // TINY_CORO_BOUND_TASK_HPP