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
#include "FunctionUtils.hpp"
#include "TupleUtils.hpp"

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

    template <std::integral auto Nth>
    using IndexedUserData = IndexedArgument<Nth, void*>;

    template <typename T, std::invocable<T>, concepts::AsyncCallbackEvent, typename>
    struct AsyncCallbackAwaiter;

    template <typename, typename, typename, typename, typename>
    struct AsyncCallbackAwaiter_CStyle;

    namespace detail {

        struct AsyncCallbackEvent
        {
            template <typename T, std::invocable<T>, concepts::AsyncCallbackEvent, typename>
            friend struct tinycoro::AsyncCallbackAwaiter;

            template <typename, typename, typename, typename, typename>
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
        : _asyncFunction{std::move(asyncFunc)}
        , _userCallback{std::move(cb)}
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
        : _asyncFunction{std::move(asyncFunc)}
        , _userCallback{std::move(cb)}
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
        UserData(void* ptr)
        : value{ptr}
        {
        }

        template <typename T>
        auto& Get()
        {
            return *static_cast<std::remove_pointer_t<T>*>(value);
        }

        void* value;
    };

    template <typename T>
    struct UserCallback
    {
        T value;
    };

    namespace detail {

        template <typename CallbackT, typename EventT = detail::AsyncCallbackEvent>
        struct UserDataWrapper
        {
            UserDataWrapper(CallbackT cb, void* d)
            : data{d}
            , userCallback{cb}
            {
            }

            void*     data;
            EventT    event{};
            CallbackT userCallback;
        };

    } // namespace detail

    namespace detail {

        template <typename T>
        struct TypeGetter
        {
            using value_type = T;
        };

        template <>
        struct TypeGetter<tinycoro::UserData>
        {
            using value_type = decltype(std::declval<tinycoro::UserData>().value);
            using is_data    = void;
        };

        template <>
        struct TypeGetter<const tinycoro::UserData>
        {
            using value_type = decltype(std::declval<tinycoro::UserData>().value);
            using is_data    = void;
        };

        template <>
        struct TypeGetter<volatile tinycoro::UserData>
        {
            using value_type = decltype(std::declval<tinycoro::UserData>().value);
            using is_data    = void;
        };

        template <>
        struct TypeGetter<const volatile tinycoro::UserData>
        {
            using value_type = decltype(std::declval<tinycoro::UserData>().value);
            using is_data    = void;
        };

        template <typename T>
        struct TypeGetter<UserCallback<T>>
        {
            using value_type  = T;
            using is_callback = void;
        };

        template <typename T>
        struct TypeGetter<const UserCallback<T>>
        {
            using value_type  = T;
            using is_callback = void;
        };

        template <typename T>
        struct TypeGetter<volatile UserCallback<T>>
        {
            using value_type  = T;
            using is_callback = void;
        };

        template <typename T>
        struct TypeGetter<const volatile UserCallback<T>>
        {
            using value_type  = T;
            using is_callback = void;
        };

        template <typename U, typename... Ts>
        constexpr auto GetCallbackIndex()
        {
            if constexpr (requires { typename TypeGetter<U>::is_callback; })
            {
                return 0;
            }
            else
            {
                if constexpr (sizeof...(Ts))
                {
                    return 1 + GetCallbackIndex<Ts...>();
                }
                return -1;
            }
        }

    } // namespace detail

    template <typename AsyncFunctionT,
              typename CallbackT,
              typename TupleT,
              typename UserDataT,
              typename ReturnT = FunctionSignature<AsyncFunctionT>::ReturnT>
    struct AsyncCallbackAwaiter_CStyle
    {
    public:
        AsyncCallbackAwaiter_CStyle(AsyncFunctionT asyncFunc, CallbackT wrappedCb, UserDataT userData, TupleT&& tuple)
        : _asyncFunction{std::move(asyncFunc)}
        , _wrappedCallback{std::move(wrappedCb)}
        , _userData{std::move(userData)}
        , _tuple{std::move(tuple)}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter_CStyle(AsyncCallbackAwaiter_CStyle&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _userData.event.Set(PauseHandler::PauseTask(hdl));

            auto convertType = [this]<typename T>(T&& t) {
                if constexpr (requires { typename detail::TypeGetter<T>::is_callback; })
                {
                    return _wrappedCallback;
                }
                else if constexpr (requires { typename detail::TypeGetter<T>::is_data; })
                {
                    return std::addressof(_userData);
                }
                else
                {
                    return std::forward<T>(t);
                }
            };

            // invoke callback
            _result = std::apply([this, &convertType]<typename... Ts>(Ts... args) { return _asyncFunction(convertType(std::forward<Ts>(args))...); },
                                 _tuple);
        }

        [[nodiscard]] auto&& await_resume() noexcept { return std::move(_result.value()); }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _wrappedCallback;
        UserDataT      _userData;
        TupleT         _tuple;

        std::optional<ReturnT> _result;
    };

    template <typename AsyncFunctionT, typename CallbackT, typename TupleT, typename UserDataT>
    struct AsyncCallbackAwaiter_CStyle<AsyncFunctionT, CallbackT, TupleT, UserDataT, void>
    {
    public:
        AsyncCallbackAwaiter_CStyle(AsyncFunctionT asyncFunc, CallbackT wrappedCb, UserDataT userData, TupleT&& tuple)
        : _asyncFunction{std::move(asyncFunc)}
        , _wrappedCallback{std::move(wrappedCb)}
        , _userData{std::move(userData)}
        , _tuple{std::move(tuple)}
        {
        }

        // disable copy and move
        AsyncCallbackAwaiter_CStyle(AsyncCallbackAwaiter_CStyle&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _userData.event.Set(PauseHandler::PauseTask(hdl));

            auto convertType = [this]<typename T>(T&& t) {
                if constexpr (requires { typename detail::TypeGetter<T>::is_callback; })
                {
                    return _wrappedCallback;
                }
                else if constexpr (requires { typename detail::TypeGetter<T>::is_data; })
                {
                    return std::addressof(_userData);
                }
                else
                {
                    return std::forward<T>(t);
                }
            };

            // invoke callback
            std::apply([this, &convertType]<typename... Ts>(Ts... args) { _asyncFunction(convertType(std::forward<Ts>(args))...); }, _tuple);
        }

        constexpr void await_resume() const noexcept { }

    private:
        AsyncFunctionT _asyncFunction;
        CallbackT      _wrappedCallback;
        UserDataT      _userData;
        TupleT         _tuple;
    };

    template <typename AsnyCallbackT, typename... Args>
    auto MakeAsyncCallbackAwaiter_CStyle(AsnyCallbackT async, Args&&... args)
    {
        constexpr auto CallbackNth = detail::GetCallbackIndex<Args...>();

        auto tempTuple = std::forward_as_tuple(args...);

        auto& callback = std::get<CallbackNth>(tempTuple).value;

        detail::UserDataWrapper userData{std::move(callback), std::get<UserData&>(tempTuple).value};

        // gets the callback signature
        using callbackSignature = FunctionSignature<decltype(callback)>;

        [[maybe_unused]] constexpr auto CustomDataNth = GetIndexFromTuple<UserData, typename callbackSignature::ArgListT>::value;

        auto cb = []<typename... Ts>(Ts... ts) {
            auto ptr = std::get<CustomDataNth>(std::forward_as_tuple(ts...));

            auto userDataPtr = static_cast<decltype(userData)*>(ptr);

            auto finalAction = Finally([userDataPtr] { userDataPtr->event.Notify(); });

            auto tuple = ReplaceTupleElement<CustomDataNth>(std::forward_as_tuple(ts...), tinycoro::UserData{userDataPtr->data});

            return std::apply([userDataPtr]<typename... Tss>(Tss&&... ts) { return userDataPtr->userCallback(std::forward<Tss>(ts)...); }, tuple);
        };

        return AsyncCallbackAwaiter_CStyle{async, cb, userData, std::move(tempTuple)};
    }

    template <typename AsnyCallbackT, typename... Args>
    auto MakeAsyncCallbackAwaiter(AsnyCallbackT async, Args&&... args)
    {
        constexpr auto CallbackNth = detail::GetCallbackIndex<Args...>();

        auto wrappedAsyncCallback = [... args = std::forward<Args>(args), async](auto wrappedCallback) {
            auto tuple = ReplaceTupleElement<CallbackNth>(std::forward_as_tuple(args...), wrappedCallback);
            return std::apply([async]<typename... Ts>(Ts&&... ts) { return async(std::forward<Ts>(ts)...); }, tuple);
        };

        auto& callback = std::get<CallbackNth>(std::forward_as_tuple(args...));
        return AsyncCallbackAwaiter{wrappedAsyncCallback, std::move(callback.value)};
    }

} // namespace tinycoro

#endif //!__TINY_CORO_ASYNC_CALLBACK_AWAITER_HPP__