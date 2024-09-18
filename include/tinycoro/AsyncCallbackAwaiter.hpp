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

    template <std::size_t N, class TupleT, class NewT>
    constexpr auto replace_tuple_element(const TupleT& t, NewT&& n)
    {
        constexpr auto tail_size = std::tuple_size<TupleT>::value - N - 1;

        return [&]<std::size_t... I_head, std::size_t... I_tail>(std::index_sequence<I_head...>, std::index_sequence<I_tail...>) {
            return std::make_tuple(std::move(std::get<I_head>(t))..., std::forward<NewT>(n), std::move(std::get<I_tail + N + 1>(t))...);
        }(std::make_index_sequence<N>{}, std::make_index_sequence<tail_size>{});
    }

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

    template <std::integral auto Nth>
    using IndexedUserData = IndexedArgument<Nth, void*>;

    template <typename T, std::invocable<T>, concepts::AsyncCallbackEvent, typename>
    struct AsyncCallbackAwaiter;

    template <std::integral auto, typename, typename, concepts::AsyncCallbackEvent, typename>
    struct AsyncCallbackAwaiter_CStyle;

    namespace detail {

        struct AsyncCallbackEvent
        {
            template <typename T, std::invocable<T>, concepts::AsyncCallbackEvent, typename>
            friend struct tinycoro::AsyncCallbackAwaiter;

            template <std::integral auto, typename, typename, concepts::AsyncCallbackEvent, typename>
            friend struct tinycoro::AsyncCallbackAwaiter_CStyle;

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

    } // namespace detail

    template <typename CallbackT,
              std::invocable<CallbackT>    AsyncFunctionT,
              concepts::AsyncCallbackEvent EventT = detail::AsyncCallbackEvent,
              typename ReturnT                    = std::invoke_result_t<AsyncFunctionT, CallbackT>>
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

    namespace detail {

        template <typename CallbackT, typename EventT = detail::AsyncCallbackEvent>
        struct UserDataWrapper : private UserData
        {
            template <std::integral auto, typename, typename, concepts::AsyncCallbackEvent, typename>
            friend struct tinycoro::AsyncCallbackAwaiter_CStyle;

            UserDataWrapper(CallbackT cb, void* data)
            : UserData{data}
            , userCallback{cb}
            {
            }

        private:
            EventT    event{};
            CallbackT userCallback;
        };

    } // namespace detail

    template <std::integral auto Nth,
              typename AsyncFunctionT,
              typename CallbackT,
              concepts::AsyncCallbackEvent EventT = detail::AsyncCallbackEvent,
              typename ReturnT                    = std::invoke_result_t<AsyncFunctionT, CallbackT, void*>>
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
                    auto forwardAsTuple = std::forward_as_tuple(ts...);

                    auto ptr = std::get<Nth>(forwardAsTuple);

                    auto userDataPtr = static_cast<decltype(_userData)*>(ptr);

                    auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });

                    auto tuple = replace_tuple_element<Nth>(forwardAsTuple, UserData::Get<void*>(userDataPtr));

                    return std::apply(
                        [userDataPtr]<typename... Args>(Args&&... args) { return userDataPtr->userCallback(std::forward<Args>(args)...); }, tuple);
                },
                std::addressof(_userData));
        }

        [[nodiscard]] auto&& await_resume() noexcept { return std::move(_result.value()); }

    private:
        AsyncFunctionT                             _asyncFunction;
        detail::UserDataWrapper<CallbackT, EventT> _userData;

        std::optional<ReturnT> _result;
    };

    template <std::integral auto Nth, typename AsyncFunctionT, typename CallbackT, concepts::AsyncCallbackEvent EventT>
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
                    auto forwardAsTuple = std::forward_as_tuple(ts...);

                    auto ptr = std::get<Nth>(forwardAsTuple);

                    auto userDataPtr = static_cast<decltype(_userData)*>(ptr);

                    auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });

                    auto tuple = replace_tuple_element<Nth>(forwardAsTuple, UserData::Get<void*>(userDataPtr));

                    return std::apply(
                        [userDataPtr]<typename... Args>(Args&&... args) { return userDataPtr->userCallback(std::forward<Args>(args)...); }, tuple);
                },
                std::addressof(_userData));
        }

        constexpr void await_resume() const noexcept { }

    private:
        AsyncFunctionT                             _asyncFunction;
        detail::UserDataWrapper<CallbackT, EventT> _userData;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__