// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP
#define TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP

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
            { e.Notify(ENotifyPolicy::RESUME) };
        };

    } // namespace concepts

    namespace detail {

        template <std::size_t N, class TupleT, class NewT>
        constexpr auto ReplaceTupleElement(TupleT&& t, NewT&& n)
        {
            constexpr auto tail_size = std::tuple_size<std::remove_reference_t<TupleT>>::value - N - 1;

            return [&]<std::size_t... I_head, std::size_t... I_tail>(std::index_sequence<I_head...>, std::index_sequence<I_tail...>) {
                return std::make_tuple(std::move(std::get<I_head>(t))..., std::forward<NewT>(n), std::move(std::get<I_tail + N + 1>(t))...);
            }(std::make_index_sequence<N>{}, std::make_index_sequence<tail_size>{});
        }

    } // namespace detail

    template <std::integral auto NthArgument, typename T>
    struct IndexedArgument
    {
        static constexpr auto value = NthArgument;

        explicit IndexedArgument(T d)
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

    template <typename CallbackT,
              std::invocable<CallbackT>    AsyncFunctionT,
              concepts::AsyncCallbackEvent EventT = detail::PauseCallbackEvent,
              typename ReturnT                    = std::invoke_result_t<AsyncFunctionT, CallbackT>>
    struct AsyncCallbackAwaiter
    {
        AsyncCallbackAwaiter(AsyncFunctionT asyncFunc, CallbackT cb)
        : _asyncFunction{std::move(asyncFunc)}
        , _userCallback{cb}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter(AsyncCallbackAwaiter&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            _event.Set(context::PauseTask(hdl));

            // invoke callback
            _result = _asyncFunction([this]<typename... Ts>(Ts... ts) {
                auto finalAction = Finally([this] { _event.Notify(); });

                if constexpr (requires (CallbackT cb) { { cb(std::forward<Ts>(ts)...) } -> std::same_as<void>; })
                {
                    try
                    {
                        // if we have no return
                        // we can handle the exception
                        _userCallback(std::forward<Ts>(ts)...);
                    }
                    catch (...)
                    {
                        // saving the exception
                        // and rethrow it later
                        _exception = std::current_exception();
                    }
                }
                else
                {
                    return _userCallback(std::forward<Ts>(ts)...);
                }
            });
        }

        [[nodiscard]] constexpr auto await_resume()
        {
            if (_exception)
            {
                // rethrow if we had an exception
                // in the completion callback
                std::rethrow_exception(_exception);
            }

            return std::move(_result.value());
        }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _userCallback;
        EventT         _event;

        std::exception_ptr _exception{};

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

        void await_suspend(auto hdl)
        {
            // put tast on pause
            _event.Set(context::PauseTask(hdl));

            // invoke callback with wrapped function
            _asyncFunction([this]<typename... Ts>(Ts... ts) {
                auto finalAction = Finally([this] { _event.Notify(); });

                if constexpr (requires (CallbackT cb) {{ cb(std::forward<Ts>(ts)...) } -> std::same_as<void>; })
                {
                    try
                    {
                        // if we have no return
                        // we can handle the exception
                        _userCallback(std::forward<Ts>(ts)...);
                    }
                    catch (...)
                    {
                        // saving the exception
                        // and rethrow it later
                        _exception = std::current_exception();
                    }
                }
                else
                {
                    return _userCallback(std::forward<Ts>(ts)...);
                }
            });
        }

        constexpr void await_resume() const
        {
            if (_exception)
            {
                // rethrow if we had an exception
                // in the completion callback
                std::rethrow_exception(_exception);
            }
        }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _userCallback;
        EventT         _event;

        std::exception_ptr _exception{};
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

        template <typename CallbackT, typename EventT = detail::PauseCallbackEvent>
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

            std::exception_ptr exception{};
        };

    } // namespace detail

    template <std::integral auto Nth,
              typename AsyncFunctionT,
              typename CallbackT,
              concepts::AsyncCallbackEvent EventT = detail::PauseCallbackEvent,
              typename ReturnT                    = std::invoke_result_t<AsyncFunctionT, CallbackT, void*>>
    struct AsyncCallbackAwaiter_CStyle
    {
        AsyncCallbackAwaiter_CStyle(AsyncFunctionT asyncFunc, CallbackT cb, IndexedUserData<Nth> userData)
        : _asyncFunction{std::move(asyncFunc)}
        , _userData{cb, userData.data}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter_CStyle(AsyncCallbackAwaiter_CStyle&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            _userData.event.Set(context::PauseTask(hdl));

            // invoke callback
            _result = _asyncFunction(
                []<typename... Ts>(Ts... ts) {
                    auto forwardAsTuple = std::forward_as_tuple(ts...);

                    auto ptr = std::get<Nth>(forwardAsTuple);

                    auto userDataPtr = static_cast<decltype(_userData)*>(ptr);

                    auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });

                    auto tuple = detail::ReplaceTupleElement<Nth>(forwardAsTuple, UserData::Get<void*>(userDataPtr));

                    return std::apply(
                        [userDataPtr]<typename... Args>(Args&&... args) {
                            if constexpr (requires (CallbackT cb) { { cb(std::forward<Args>(args)...) } -> std::same_as<void>; })
                            {
                                try
                                {
                                    // if we have no return
                                    // we can handle the exception
                                    userDataPtr->userCallback(std::forward<Args>(args)...);
                                }
                                catch (...)
                                {
                                    // saving the exception
                                    // and rethrow it later
                                    userDataPtr->exception = std::current_exception();
                                }
                            }
                            else
                            {
                                return userDataPtr->userCallback(std::forward<Args>(args)...);
                            }
                        },
                        tuple);
                },
                std::addressof(_userData));
        }

        [[nodiscard]] constexpr auto await_resume()
        {
            if (_userData.exception)
            {
                // rethrow if we had an exception
                // in the completion callback
                std::rethrow_exception(_userData.exception);
            }

            return std::move(_result.value());
        }

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

        void await_suspend(auto hdl)
        {
            // put tast on pause
            _userData.event.Set(context::PauseTask(hdl));

            // invoke callback
            _asyncFunction(
                []<typename... Ts>(Ts... ts) {
                    auto forwardAsTuple = std::forward_as_tuple(ts...);

                    auto ptr = std::get<Nth>(forwardAsTuple);

                    auto userDataPtr = static_cast<decltype(_userData)*>(ptr);

                    auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });

                    auto tuple = detail::ReplaceTupleElement<Nth>(forwardAsTuple, UserData::Get<void*>(userDataPtr));

                    return std::apply(
                        [userDataPtr]<typename... Args>(Args&&... args) {
                            if constexpr (requires (CallbackT cb) { { cb(std::forward<Args>(args)...) } -> std::same_as<void>; })
                            {
                                try
                                {
                                    // if we have no return
                                    // we can handle the exception
                                    userDataPtr->userCallback(std::forward<Args>(args)...);
                                }
                                catch (...)
                                {
                                    // saving the exception
                                    // and rethrow it later
                                    userDataPtr->exception = std::current_exception();
                                }
                            }
                            else
                            {
                                return userDataPtr->userCallback(std::forward<Args>(args)...);
                            }
                        },
                        tuple);
                },
                std::addressof(_userData));
        }

        constexpr void await_resume() const
        {
            if (_userData.exception)
            {
                // rethrow if we had an exception
                // in the completion callback
                std::rethrow_exception(_userData.exception);
            }
        }

    private:
        AsyncFunctionT                             _asyncFunction;
        detail::UserDataWrapper<CallbackT, EventT> _userData;
    };

} // namespace tinycoro

#endif // TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP