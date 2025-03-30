#ifndef __TINY_CORO_CORO_TASK_HPP__
#define __TINY_CORO_CORO_TASK_HPP__

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

    struct DestroyNotifier
    {
        DestroyNotifier() = default;

        template <std::regular_invocable T>
        DestroyNotifier(T&& callback)
        : _notifier{std::forward<T>(callback)}
        {
        }

        DestroyNotifier(DestroyNotifier&& other) noexcept
        : _notifier{std::exchange(other._notifier, nullptr)}
        {
        }

        DestroyNotifier& operator=(DestroyNotifier&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                _notifier = std::exchange(other._notifier, nullptr);
            }
            return *this;
        }

        template <std::regular_invocable T>
        void Set(T&& callback)
        {
            _notifier = std::forward<T>(callback);
        }

        void Notify() const noexcept
        {
            if (_notifier)
            {
                std::invoke(_notifier);
            }
        }

    private:
        std::function<void()> _notifier;
    };

    template <typename ReturnValueT,
              typename PromiseT,
              template <typename, typename> class AwaiterT,
              typename CoroResumerT     = TaskResumer,
              typename StopSourceT      = std::stop_source,
              typename DestroyNotifierT = DestroyNotifier>
    struct CoroTask : private AwaiterT<ReturnValueT, CoroTask<ReturnValueT, PromiseT, AwaiterT, CoroResumerT, StopSourceT>>
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

        using value_type = promise_type::value_type;

        template <typename... Args>
            requires std::constructible_from<coro_hdl_type, Args...>
        CoroTask(Args&&... args)
        : _hdl{std::forward<Args>(args)...}
        {
        }

        CoroTask(CoroTask&& other) noexcept
        : _hdl{std::exchange(other._hdl, nullptr)}
        , _source{std::exchange(other._source, StopSourceT{std::nostopstate})}
        , _destroyNotifier{std::move(other._destroyNotifier)}
        {
        }

        CoroTask& operator=(CoroTask&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                destroy();
                _hdl             = std::exchange(other._hdl, nullptr);
                _source          = std::exchange(other._source, StopSourceT{std::nostopstate});
                _destroyNotifier = std::move(other._destroyNotifier);
            }
            return *this;
        }

        ~CoroTask() { destroy(); }

        void Resume() { std::invoke(_coroResumer, _hdl, _source); }

        [[nodiscard]] auto ResumeState() { return _coroResumer.ResumeState(_hdl, _source); }

        [[nodiscard]] bool IsPaused() const noexcept { return _hdl.promise().pauseHandler->IsPaused(); }

        [[nodiscard]] bool IsDone() const noexcept { return _hdl.done(); }

        auto SetPauseHandler(concepts::PauseHandlerCb auto pauseResume)
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
            _source                   = std::forward<T>(arg);
            _hdl.promise().stopSource = _source;
        }

        template <std::regular_invocable T>
        void SetDestroyNotifier(T&& cb)
        {
            _destroyNotifier.Set(std::forward<T>(cb));
        }

        [[nodiscard]] address_t Address() const noexcept { return _hdl.address(); }

    private:
        void destroy()
        {
            if (_hdl)
            {
                _source.request_stop();
                _hdl.destroy();
                _hdl = nullptr;
                _destroyNotifier.Notify();
            }
        }

        [[no_unique_address]] CoroResumerT _coroResumer{};
        coro_hdl_type                      _hdl;
        StopSourceT                        _source{std::nostopstate};
        DestroyNotifierT                   _destroyNotifier;
    };

    template <typename ReturnValueT = void>
    using Task = CoroTask<ReturnValueT, Promise<ReturnValueT>, AwaiterValue>;

} // namespace tinycoro

#endif //!__TINY_CORO_CORO_TASK_HPP__