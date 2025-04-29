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

namespace tinycoro {

    template <typename ReturnValueT,
              typename PromiseT,
              template <typename, typename> class AwaiterT,
              typename CoroResumerT = TaskResumer,
              typename StopSourceT  = std::stop_source>
    struct [[nodiscard]] CoroTask : private AwaiterT<ReturnValueT, CoroTask<ReturnValueT, PromiseT, AwaiterT, CoroResumerT, StopSourceT>>
    {
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
        , _triggerStopOnExit{std::exchange(other._triggerStopOnExit, false)}
        , _destroyNotifier{std::move(other._destroyNotifier)}
        {
        }

        CoroTask& operator=(CoroTask&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                destroy();
                _hdl             = std::exchange(other._hdl, nullptr);
                _triggerStopOnExit = std::exchange(other._triggerStopOnExit, false);
                _destroyNotifier = std::move(other._destroyNotifier);
            }
            return *this;
        }

        ~CoroTask() { destroy(); }

        // Resumes the coroutine
        inline void Resume() { _coroResumer.Resume(_hdl/*, _source*/); }

        [[nodiscard]] auto ResumeState() noexcept { return _coroResumer.ResumeState(_hdl/*, _source*/); }

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
            _hdl.promise().stopSource = std::forward<T>(arg);
            _triggerStopOnExit = true;
        }

        template <std::regular_invocable T>
        void SetDestroyNotifier(T&& cb) noexcept
        {
            _destroyNotifier = std::forward<T>(cb);
        }

        [[nodiscard]] address_t Address() const noexcept { return _hdl.address(); }

    private:
        void destroy() noexcept
        {
            if (_hdl)
            {
                if(_triggerStopOnExit)
                {
                    // only trigger stop, if the
                    // stop_source was set explicitly.
                    _hdl.promise().stopSource.request_stop();
                }

                _hdl.destroy();
                _hdl = nullptr;

                if (_destroyNotifier)
                {
                    // notify others that the task
                    // is destroyed.
                    _destroyNotifier();
                }
            }
        }

        // contains special logic regarging
        // coroutine resumption and state
        [[no_unique_address]] CoroResumerT _coroResumer{};

        // The underlying coroutine_handle
        coro_hdl_type _hdl;

        // In case this is "true", the stop source
        // was explicitly set and need to be triggered
        // in the destroy().
        bool _triggerStopOnExit{false};

        // callback to notify others if
        // the coroutine is destroyed.
        std::function<void()> _destroyNotifier;
    };

    template <typename ReturnValueT = void>
    using Task = CoroTask<ReturnValueT, Promise<ReturnValueT>, AwaiterValue>;

} // namespace tinycoro

#endif // TINY_CORO_CORO_TASK_HPP