#ifndef __TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__
#define __TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__

#include <concepts>
#include <optional>
#include <functional>
#include <coroutine>
#include <type_traits>
#include <cassert>

#include "PauseHandler.hpp"
#include "Finally.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename E>
        concept AsyncCallbackEvent = requires (E e) {
            { e.Notify() };
        };

    } // namespace concepts

    template <std::integral auto NthArgument, typename T>
    struct IndexedArgument
    {
        static constexpr auto value = NthArgument;

        IndexedArgument(T d)
        : data{d}
        {
        }

        T data;
    };

    template<std::integral auto Nth>
    using IndexedUserData = IndexedArgument<Nth, void*>;

    template <typename T, std::invocable<T>, concepts::AsyncCallbackEvent, typename>
    struct AsyncCallbackAwaiter;

    template <std::integral auto, typename, typename, concepts::AsyncCallbackEvent, typename>
    struct AsyncCallbackAwaiter_CStyle;

    struct AsyncCallbackEvent
    {
        template <typename T, std::invocable<T>, concepts::AsyncCallbackEvent, typename>
        friend struct AsyncCallbackAwaiter;

        template <std::integral auto, typename, typename, concepts::AsyncCallbackEvent, typename>
        friend struct AsyncCallbackAwaiter_CStyle;

        void Notify() const 
        {
            if (_notifyCallback)
            {
                _notifyCallback();
            }
        }

    private:
        void Set(std::invocable auto cb)
        {
            assert(_notifyCallback == nullptr);

            _notifyCallback = cb;
        }

        std::function<void()> _notifyCallback;
    };

    template <typename CallbackT,
              std::invocable<CallbackT> AsyncFunctionT,
              concepts::AsyncCallbackEvent           EventT = AsyncCallbackEvent,
              typename ReturnT                 = std::invoke_result_t<AsyncFunctionT, CallbackT>>
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
            // put tast on pause
            _event.Set(PauseHandler::PauseTask(hdl));

            // invoke callback
            _result = _asyncFunction([this]<typename... Ts>(Ts... ts) {
                auto finalAction = Finally([this] { _event.Notify(); });
                return _userCallback(std::forward<Ts>(ts)...);
            });
        }

        [[nodiscard]] auto&& await_resume() noexcept { return std::move(_result.value()); }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _userCallback;
        EventT         _event;

        std::optional<ReturnT> _result;
    };

    template <typename CallbackT, std::invocable<CallbackT> AsyncFunctionT, concepts::AsyncCallbackEvent EventT>
    struct AsyncCallbackAwaiter<CallbackT, AsyncFunctionT, EventT, void>
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
            // put tast on pause
            _event.Set(PauseHandler::PauseTask(hdl));

            // invoke callback with wrapped function
            _asyncFunction([this]<typename... Ts>(Ts... ts) {
                auto finalAction = Finally([this] { _event.Notify(); });
                return _userCallback(std::forward<Ts>(ts)...);
            });
        }

        void await_resume() noexcept { }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _userCallback;
        EventT         _event;
    };

    struct UserData
    {
        constexpr UserData(void* data)
        : _userData{data}
        {
        }

        template <typename T>
        static auto Get(void* userDataClass)
        {
            auto data = static_cast<tinycoro::UserData*>(userDataClass);
            return static_cast<std::remove_pointer_t<T>*>(data->_userData);
        }

    private:
        void* _userData;
    };

    template <typename CallbackT, typename EventT = AsyncCallbackEvent>
    struct UserDataWrapper : private UserData
    {
        template <std::integral auto, typename, typename, concepts::AsyncCallbackEvent, typename>
        friend struct AsyncCallbackAwaiter_CStyle;

        UserDataWrapper(CallbackT cb, void* data)
        : UserData{data}
        , userCallback{cb}
        {
        }

    private:
        EventT    event{};
        CallbackT userCallback;
    };

    template <std::integral auto Nth,
              typename AsyncFunctionT,
              typename CallbackT,
              concepts::AsyncCallbackEvent EventT = AsyncCallbackEvent,
              typename ReturnT       = std::invoke_result_t<AsyncFunctionT, CallbackT, void*>>
    struct AsyncCallbackAwaiter_CStyle
    {
        AsyncCallbackAwaiter_CStyle(AsyncFunctionT asyncFunc, CallbackT cb, IndexedUserData<Nth> userData)
        : _asyncFunction{asyncFunc}
        , _userData{cb, userData.data}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter_CStyle(AsyncCallbackAwaiter_CStyle&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _userData.event.Set(PauseHandler::PauseTask(hdl));

            // invoke callback
            _result = _asyncFunction(
                []<typename... Ts>(Ts... ts) {
                    auto ptr = std::get<Nth>(std::forward_as_tuple(ts...));

                    auto userDataPtr = static_cast<decltype(_userData)*>(ptr);

                    auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });
                    return userDataPtr->userCallback(std::forward<Ts>(ts)...);
                },
                std::addressof(_userData));
        }

        [[nodiscard]] auto&& await_resume() noexcept { return std::move(_result.value()); }

    private:
        AsyncFunctionT                     _asyncFunction;
        UserDataWrapper<CallbackT, EventT> _userData;

        std::optional<ReturnT> _result;
    };

    template <std::integral auto Nth,
              typename AsyncFunctionT,
              typename CallbackT,
              concepts::AsyncCallbackEvent EventT>
    struct AsyncCallbackAwaiter_CStyle<Nth, AsyncFunctionT, CallbackT, EventT, void>
    {
        AsyncCallbackAwaiter_CStyle(AsyncFunctionT asyncFunc, CallbackT cb, IndexedUserData<Nth> userData)
        : _asyncFunction{asyncFunc}
        , _userData{cb, userData.data}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter_CStyle(AsyncCallbackAwaiter_CStyle&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _userData.event.Set(PauseHandler::PauseTask(hdl));

            // invoke callback
            _asyncFunction(
                []<typename... Ts>(Ts... ts) {
                    auto ptr = std::get<Nth>(std::forward_as_tuple(ts...));

                    auto userDataPtr = static_cast<decltype(_userData)*>(ptr);

                    auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });
                    return userDataPtr->userCallback(std::forward<Ts>(ts)...);
                },
                std::addressof(_userData));
        }

        constexpr void await_resume() const noexcept { }

    private:
        AsyncFunctionT                     _asyncFunction;
        UserDataWrapper<CallbackT, EventT> _userData;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__