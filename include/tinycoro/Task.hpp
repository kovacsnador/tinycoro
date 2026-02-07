// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CORO_TASK_HPP
#define TINY_CORO_CORO_TASK_HPP

#include <chrono>
#include <coroutine>
#include <optional>
#include <cstddef>
#include <stop_token>
#include <utility>

#include "Common.hpp"
#include "Exception.hpp"
#include "Promise.hpp"
#include "TaskAwaiter.hpp"
#include "TaskResumer.hpp"
#include "BoundTask.hpp"

namespace tinycoro {

    namespace detail {

        template <typename ReturnValueT,
                  concepts::IsInitialCancellablePolicy InitialCancellablePolicyT,
                  typename PromiseT,
                  template <typename, typename> class AwaiterT,
                  typename CoroResumerT = TaskResumer,
                  typename StopSourceT  = std::stop_source>
        struct [[nodiscard]] CoroTask
        : private AwaiterT<ReturnValueT, CoroTask<ReturnValueT, InitialCancellablePolicyT, PromiseT, AwaiterT, CoroResumerT, StopSourceT>>
        {
            template <typename CoroutineFunctionT, typename... Args>
            friend auto tinycoro::MakeBound(CoroutineFunctionT&& func, Args&&... args);

            using SelfType = CoroTask<ReturnValueT, InitialCancellablePolicyT, PromiseT, AwaiterT, CoroResumerT, StopSourceT>;

            friend struct AwaiterBase<SelfType>;
            friend class AwaiterT<ReturnValueT, SelfType>;

            using awaiter_type = AwaiterT<ReturnValueT, SelfType>;

            using awaiter_type::await_ready;
            using awaiter_type::await_resume;
            using awaiter_type::await_suspend;

            using promise_type  = PromiseT;
            using coro_hdl_type = std::coroutine_handle<promise_type>;

            using value_type = typename promise_type::value_type;

            using initial_cancellable_policy_t = InitialCancellablePolicyT;

            // default constructor
            constexpr CoroTask() = default;

            template <typename... Args>
                requires std::constructible_from<coro_hdl_type, Args...> && (sizeof...(Args) > 0)
            CoroTask(Args&&... args)
            : _hdl{std::forward<Args>(args)...}
            {
                assert(_hdl);

                // Warning!
                //
                // Do not call promise() here.
                // On some compilers (especially Clang 17 and above), this is dangerous.
                // In Release mode, the optimizer may remove parts of the code,
                // which can lead to crashes.
            }

            CoroTask(CoroTask&& other) noexcept
            : _hdl{std::exchange(other._hdl, nullptr)}
            {
            }

            CoroTask& operator=(CoroTask&& other) noexcept
            {
                CoroTask{std::move(other)}.swap(*this);
                return *this;
            }

            ~CoroTask() { destroy(); }

            // Resumes the coroutine
            inline void Resume() { _coroResumer.Resume(_hdl.promise()); }

            [[nodiscard]] auto ResumeState() noexcept { return _coroResumer.ResumeState(_hdl); }

            [[nodiscard]] bool IsPaused() const noexcept { return SharedState()->IsPaused(); }

            [[nodiscard]] bool IsDone() const noexcept { return _hdl.done(); }

            void SetResumeCallback(concepts::IsResumeCallbackType auto pauseResume) noexcept
            {
                auto sharedStatePtr = SharedState();

                assert(sharedStatePtr);

                // pause handler is already initialized
                sharedStatePtr->ResetCallback(std::move(pauseResume));
            }

            [[nodiscard]] auto* SharedState() noexcept { return _hdl.promise().SharedState(); }
            [[nodiscard]] auto* SharedState() const noexcept { return _hdl.promise().SharedState(); }

            template <typename T>
                requires std::constructible_from<StopSourceT, T>
            void SetStopSource(T&& arg)
            {
                _hdl.promise().SetStopSource(std::forward<T>(arg));
            }

            void SetCustomData(void* awaitable) noexcept { _hdl.promise().SetCustomData(awaitable); }

            [[nodiscard]] detail::address_t Address() const noexcept { return _hdl.address(); }

            // Release the coroutine_handle object
            [[nodiscard]] constexpr auto Release() noexcept { return std::exchange(_hdl, nullptr); }

            constexpr void swap(CoroTask& other) noexcept { std::swap(other._hdl, _hdl); }

        private:
            // Only used by MakeBound() to save
            // the coroutine function inside the
            // coroutine promise
            template <typename T>
            void SaveCoroutineFunction(T&& function) noexcept
            {
                _hdl.promise().SaveAnyFunction(std::forward<T>(function));
            }

            void destroy() noexcept
            {
                if (_hdl)
                {
                    _hdl.destroy();
                    _hdl = nullptr;
                }
            }

            // contains special logic regarging
            // coroutine resumption and state
            [[no_unique_address]] CoroResumerT _coroResumer{};

            // The underlying coroutine_handle
            coro_hdl_type _hdl{nullptr};
        };

    } // namespace detail

    template <typename ReturnT                                               = void,
              template <typename> class AllocatorT                           = DefaultAllocator,
              concepts::IsInitialCancellablePolicy InitialCancellablePolicyT = default_initial_cancellable_policy>
    using Task = detail::CoroTask<ReturnT, InitialCancellablePolicyT, detail::Promise<ReturnT, InitialCancellablePolicyT, AllocatorT>, AwaiterValue>;

    template <typename ReturnT                                               = void,
              template <typename> class AllocatorT                           = DefaultAllocator,
              concepts::IsInitialCancellablePolicy InitialCancellablePolicyT = default_initial_cancellable_policy>
    using InlineTask
        = detail::CoroTask<ReturnT, InitialCancellablePolicyT, detail::InlinePromise<ReturnT, InitialCancellablePolicyT, AllocatorT>, AwaiterValue>;

    // Convenience aliases for tasks with a non-initial cancellable policy.
    // (TaskNIC/InlineTaskNIC)
    //
    // These are helper types meant to simplify usage when cancellation should not be
    // automatically propagated into the coroutine on creation (i.e., not initially cancellable).
    // This is typically useful for root coroutines or tasks that should explicitly manage
    // cancellation behavior.
    //
    // Note: These aliases are just syntactic sugar over Task and InlineTask with
    // `noninitial_cancellable_t` passed as the cancellation policy.
    template <typename ReturnT = void, template <typename> class AllocatorT = DefaultAllocator>
    using TaskNIC = Task<ReturnT, AllocatorT, noninitial_cancellable_t>;

    template <typename ReturnT = void, template <typename> class AllocatorT = DefaultAllocator>
    using InlineTaskNIC = InlineTask<ReturnT, AllocatorT, noninitial_cancellable_t>;

} // namespace tinycoro

#endif // TINY_CORO_CORO_TASK_HPP