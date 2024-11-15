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

    struct DestroyNofifier
    {
        DestroyNofifier() = default;

        template<std::regular_invocable T>
        DestroyNofifier(T&& callback)
        : _notifier{std::forward<T>(callback)}
        {
        }

        DestroyNofifier(DestroyNofifier&& other) noexcept
        : _notifier{std::exchange(other._notifier, nullptr)}
        {
        }

        DestroyNofifier& operator=(DestroyNofifier&& other) noexcept
        {
            if(std::addressof(other) != this)
            {
                _notifier = std::exchange(other._notifier, nullptr);
            }
            return *this;
        }

        template<std::regular_invocable T>
        void Set(T&& callback)
        {
            _notifier = std::forward<T>(callback);
        }

        void Notify() const noexcept
        {
            if(_notifier)
            {
                std::invoke(_notifier);
            }
        }

    private:
        std::function<void()> _notifier;
    };

    template <typename PromiseT, typename AwaiterT, typename CoroResumerT = TaskResumer, typename StopSourceT = std::stop_source, typename DestroyNotifierT = DestroyNofifier>
    struct CoroTaskView : private AwaiterT
    {
        using promise_type  = PromiseT;
        using coro_hdl_type = std::coroutine_handle<promise_type>;

        using AwaiterT::await_ready;
        using AwaiterT::await_resume;
        using AwaiterT::await_suspend;

        CoroTaskView(coro_hdl_type hdl, StopSourceT source)
        : _hdl{hdl}
        , _source{source}
        {
        }

        CoroTaskView(CoroTaskView&& other) noexcept
        : _hdl{std::exchange(other._hdl, nullptr)}
        , _source{std::exchange(other._source, StopSourceT{std::nostopstate})}
        , _destroyNotifier{std::move(other._destroyNotifier)}
        {
        }

        CoroTaskView& operator=(CoroTaskView&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                destroy();
                _hdl    = std::exchange(other._hdl, nullptr);
                _source = std::exchange(other._source, StopSourceT{std::nostopstate});
                _destroyNotifier = std::move(other._destroyNotifier);
            }
            return *this;
        }

        ~CoroTaskView() { destroy(); }

        [[nodiscard]] auto Resume() { return std::invoke(_coroResumer, _hdl, _source); }

        void SetPauseHandler(concepts::PauseHandlerCb auto pauseResume)
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                using elementType = typename std::pointer_traits<decltype(_hdl.promise().pauseHandler)>::element_type;
                _hdl.promise().pauseHandler = std::make_shared<elementType>(pauseResume);
            }
        }

        bool IsPaused() const noexcept
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                if(_hdl.promise().pauseHandler)
                {
                    return _hdl.promise().pauseHandler->IsPaused();
                }
            }
            return false;
        }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            _source                   = std::forward<T>(arg);
            _hdl.promise().stopSource = _source;
        }

        template<std::regular_invocable T>
        void SetDestroyNotifier(T&& cb)
        {
            _destroyNotifier.Set(std::forward<T>(cb));
        }

    private:
        void destroy() { 
            _source.request_stop();
            _destroyNotifier.Notify();
        }

        [[no_unique_address]] CoroResumerT _coroResumer{};
        coro_hdl_type                      _hdl;
        StopSourceT                        _source{std::nostopstate};
        DestroyNotifierT                   _destroyNotifier;
    };

    template <typename ReturnValueT,
              typename PromiseT,
              template <typename, typename>
              class AwaiterT,
              typename CoroResumerT = TaskResumer,
              typename StopSourceT  = std::stop_source,
              typename DestroyNotifierT = DestroyNofifier>
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
                _hdl    = std::exchange(other._hdl, nullptr);
                _source = std::exchange(other._source, StopSourceT{std::nostopstate});
                _destroyNotifier = std::move(other._destroyNotifier);
            }
            return *this;
        }

        ~CoroTask() { destroy(); }

        [[nodiscard]] auto Resume() { return std::invoke(_coroResumer, _hdl, _source); }

        void SetPauseHandler(concepts::PauseHandlerCb auto pauseResume)
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                using elementType = typename std::pointer_traits<decltype(_hdl.promise().pauseHandler)>::element_type;
                _hdl.promise().pauseHandler = std::make_shared<elementType>(pauseResume);
            }
        }

        bool IsPaused() const noexcept
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                if(_hdl.promise().pauseHandler)
                {
                    return _hdl.promise().pauseHandler->IsPaused();
                }
            }
            return false;
        }

        [[nodiscard]] auto TaskView() const noexcept { return CoroTaskView<promise_type, awaiter_type, CoroResumerT>{_hdl, _source}; }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            _source                   = std::forward<T>(arg);
            _hdl.promise().stopSource = _source;
        }

        template<std::regular_invocable T>
        void SetDestroyNotifier(T&& cb)
        {
            _destroyNotifier.Set(std::forward<T>(cb));
        }

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

    template <typename ReturnValueT>
    using Task = CoroTask<ReturnValueT, Promise<ReturnValueT>, AwaiterValue>;

} // namespace tinycoro

#endif //!__TINY_CORO_CORO_TASK_HPP__