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

#include "Common.hpp"
#include "Exception.hpp"
#include "PauseHandler.hpp"
#include "Promise.hpp"
#include "TaskAwaiter.hpp"
#include "TaskResumer.hpp"
#include "BoundTask.hpp"

namespace tinycoro {

    namespace detail {

        template <typename ReturnValueT,
                  typename PromiseT,
                  template <typename, typename> class AwaiterT,
                  typename CoroResumerT = TaskResumer,
                  typename StopSourceT  = std::stop_source>
        struct [[nodiscard]] CoroTask : private AwaiterT<ReturnValueT, CoroTask<ReturnValueT, PromiseT, AwaiterT, CoroResumerT, StopSourceT>>
        {
            template <typename CoroutineFunctionT, typename... Args>
            friend auto tinycoro::MakeBound(CoroutineFunctionT&& func, Args&&... args);

            using SelfType = CoroTask<ReturnValueT, PromiseT, AwaiterT, CoroResumerT, StopSourceT>;

            friend struct AwaiterBase<SelfType>;
            friend class AwaiterT<ReturnValueT, SelfType>;

            using awaiter_type = AwaiterT<ReturnValueT, SelfType>;

            using awaiter_type::await_ready;
            using awaiter_type::await_resume;
            using awaiter_type::await_suspend;

            using promise_type  = PromiseT;
            using coro_hdl_type = std::coroutine_handle<promise_type>;

            using value_type = typename promise_type::value_type;

            template <typename... Args>
                requires std::constructible_from<coro_hdl_type, Args...>
            CoroTask(Args&&... args)
            : _hdl{std::forward<Args>(args)...}
            {
            }

            CoroTask(CoroTask&& other) noexcept
            : _hdl{std::exchange(other._hdl, nullptr)}
            {
            }

            CoroTask& operator=(CoroTask&& other) noexcept
            {
                if (std::addressof(other) != this)
                {
                    destroy();
                    _hdl = std::exchange(other._hdl, nullptr);
                }
                return *this;
            }

            ~CoroTask() { destroy(); }

            // Resumes the coroutine
            inline void Resume() { _coroResumer.Resume(_hdl.promise()); }

            [[nodiscard]] auto ResumeState() noexcept { return _coroResumer.ResumeState(_hdl); }

            [[nodiscard]] bool IsPaused() const noexcept { return _hdl.promise().pauseHandler->IsPaused(); }

            [[nodiscard]] bool IsDone() const noexcept { return _hdl.done(); }

            auto SetPauseHandler(concepts::PauseHandlerCb auto pauseResume) noexcept
            {
                auto& pauseHandler = _hdl.promise().pauseHandler;
                if (pauseHandler)
                {
                    // pause handler is already initialized
                    pauseHandler->ResetCallback(std::move(pauseResume));
                }
                else
                {
                    // pause handler need to be initialized
                    _hdl.promise().MakePauseHandler(std::move(pauseResume));
                }

                return pauseHandler.get();
            }

            [[nodiscard]] auto* GetPauseHandler() noexcept { return _hdl.promise().pauseHandler.get(); }

            template <typename T>
                requires std::constructible_from<StopSourceT, T>
            void SetStopSource(T&& arg)
            {
                _hdl.promise().SetStopSource(std::forward<T>(arg));
            }

            template <std::regular_invocable T>
            void SetDestroyNotifier(T&& cb) noexcept
            {
                _hdl.promise().SetDestroyNotifier(std::forward<T>(cb));
            }

            [[nodiscard]] address_t Address() const noexcept { return _hdl.address(); }

            // Release the coroutine_handle object
            auto Release() noexcept { return std::exchange(_hdl, nullptr); }

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
                if(_hdl)
                {
                    _hdl.destroy();
                    _hdl = nullptr;
                }
            }

            // contains special logic regarging
            // coroutine resumption and state
            [[no_unique_address]] CoroResumerT _coroResumer{};

            // The underlying coroutine_handle
            coro_hdl_type _hdl;
        };

    } // namespace detail

    template <typename ReturnValueT = void, template<typename> class AllocatorT = detail::NonAllocatorAdapter>
    using Task = detail::CoroTask<ReturnValueT, Promise<ReturnValueT, AllocatorT>, AwaiterValue>;

    template <typename ReturnValueT = void, template<typename> class AllocatorT = detail::NonAllocatorAdapter>
    using InlineTask = detail::CoroTask<ReturnValueT, InlinePromise<ReturnValueT, AllocatorT>, AwaiterValue>;

} // namespace tinycoro

#endif // TINY_CORO_CORO_TASK_HPP