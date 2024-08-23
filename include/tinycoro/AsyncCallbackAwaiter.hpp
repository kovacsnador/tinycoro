#ifndef __TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__
#define __TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__

#include <concepts>
#include <optional>
#include <functional>
#include <coroutine>
#include <type_traits>
#include <cassert>

#include "Common.hpp"

namespace tinycoro {

    template <typename PromiseT>
    std::function<void()> PauseTask(std::coroutine_handle<PromiseT>& hdl)
    {
        hdl.promise().paused = true;
        return [hdl] { hdl.promise().pauseResume(); };
    }

    namespace concepts {
        template <typename E>
        concept Event = requires (E e) {
            { e.Notify() };
        };
    } // namespace concepts

    template <typename, typename, concepts::Event, typename, typename>
    struct AsyncCallbackAwaiter;

    struct Event
    {
        template <typename, typename, concepts::Event, typename, typename>
        friend struct AsyncCallbackAwaiter;

        void Notify()
        {
            if (_done)
            {
                _done();
                _done = nullptr;
            }
        }

    private:
        void Set(std::invocable auto cb)
        {
            assert(_done == nullptr);
            _done = cb;
        }

        std::function<void()> _done;
    };

    template <typename AsyncFunctionT,
              typename CallbackT,
              concepts::Event EventT = Event,
              typename ReturnT       = std::invoke_result_t<AsyncFunctionT, CallbackT>,
              typename DummyLambdaT  = decltype([] {})>
    struct AsyncCallbackAwaiter
    {
        AsyncCallbackAwaiter(AsyncFunctionT asyncFunc, CallbackT cb)
        : _asyncFunction{asyncFunc}
        , _userCallback{cb}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter(AsyncCallbackAwaiter&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            static EventT    s_event;
            static CallbackT s_callback{_userCallback};

            // put tast on pause
            s_event.Set(PauseTask(hdl));

            // invoke callback
            _result = _asyncFunction([]<typename... Ts>(Ts... ts) {
                s_callback(std::forward<Ts>(ts)...);
                s_event.Notify();
            });
        }

        [[nodiscard]] auto&& await_resume() noexcept { return std::move(_result.value()); }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _userCallback;

        std::optional<ReturnT> _result;
    };

    template <typename AsyncFunctionT, typename CallbackT, concepts::Event EventT, typename DummyLambdaT>
    struct AsyncCallbackAwaiter<AsyncFunctionT, CallbackT, EventT, void, DummyLambdaT>
    {
        AsyncCallbackAwaiter(AsyncFunctionT asyncFunc, CallbackT cb)
        : _asyncFunction{asyncFunc}
        , _userCallback{cb}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter(AsyncCallbackAwaiter&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            static EventT    s_event;
            static CallbackT s_callback{_userCallback};

            // put tast on pause
            s_event.Set(PauseTask(hdl));

            // invoke callback
            _asyncFunction([]<typename... Ts>(Ts... ts) {
                s_callback(std::forward<Ts>(ts)...);
                s_event.Notify();
            });
        }

        void await_resume() const noexcept { }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _userCallback;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__