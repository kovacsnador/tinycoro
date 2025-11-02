// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_DETACHED_TASK_HPP
#define TINY_CORO_DETACHED_TASK_HPP

#include "Common.hpp"

namespace tinycoro {

    template <typename TaskT>
    struct Detach
    {
        using value_type = TaskT::value_type;
        using promise_type = TaskT::promise_type;
        using initial_cancellable_policy_t = TaskT::initial_cancellable_policy_t;

        Detach(TaskT&& task)
        : _task{task}
        {
        }

        [[nodiscard]] auto Address() const noexcept { return _task.Address(); }
        [[nodiscard]] constexpr auto Release() noexcept { return _task.Release(); }

    private:
        TaskT& _task;
    };

    namespace detail {
        
        template <typename T>
        struct IsDetached : std::false_type
        {
        };

        template <typename T>
        struct IsDetached<Detach<T>> : std::true_type
        {
        };

    } // namespace detail

} // namespace tinycoro

#endif // TINY_CORO_DETACHED_TASK_HPP