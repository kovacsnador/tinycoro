#ifndef __TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__
#define __TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__

#include <concepts>
#include <optional>
#include <functional>
#include <coroutine>
#include <type_traits>
#include <cassert>

#include "PauseHandler.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename E>
        concept Event = requires (E e) {
            { e.Notify() };
        };

    } // namespace concepts

    template <std::regular_invocable, concepts::Event, typename>
    struct AsyncCallbackAwaiter;

    struct Event
    {
        template <std::regular_invocable, concepts::Event, typename>
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

    template <std::regular_invocable AsyncFunctionT, concepts::Event EventT, typename ReturnT = std::invoke_result_t<AsyncFunctionT>>
    struct AsyncCallbackAwaiter
    {
        AsyncCallbackAwaiter(AsyncFunctionT asyncFunc, EventT& event)
        : _asyncFunction{asyncFunc}
        , _event{event}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter(AsyncCallbackAwaiter&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _event.Set(PauseHandler::PauseTask(hdl));

            // invoke callback
            _result = _asyncFunction();
        }

        [[nodiscard]] auto&& await_resume() noexcept { return std::move(_result.value()); }

    private:
        AsyncFunctionT _asyncFunction;
        EventT&        _event;

        std::optional<ReturnT> _result;
    };

    template <std::regular_invocable AsyncFunctionT, concepts::Event EventT>
    struct AsyncCallbackAwaiter<AsyncFunctionT, EventT, void>
    {
        AsyncCallbackAwaiter(AsyncFunctionT asyncFunc, EventT& event)
        : _asyncFunction{asyncFunc}
        , _event{event}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter(AsyncCallbackAwaiter&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _event.Set(PauseHandler::PauseTask(hdl));

            // invoke callback
            _asyncFunction();
        }

        constexpr void await_resume() noexcept { }

    private:
        AsyncFunctionT _asyncFunction;
        EventT&        _event;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__