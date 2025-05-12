#ifndef TINY_CORO_PROMISE_NEW_HPP
#define TINY_CORO_PROMISE_NEW_HPP

#include <coroutine>
#include <future>

#include "PromiseBase.hpp"
#include "PauseHandler.hpp"

namespace tinycoro {
    namespace detail {

        struct FinalAwaiter
        {
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            [[nodiscard]] std::coroutine_handle<> await_suspend(auto hdl) noexcept
            {
                auto& promise = hdl.promise();

                if (promise.parent)
                {
                    using promise_t = std::remove_pointer_t<decltype(promise.parent)>;

                    promise.parent->child = nullptr;
                    return std::coroutine_handle<promise_t>::from_promise(*promise.parent);
                }

                return std::noop_coroutine();
            }

            void await_resume() const noexcept { }
        };

        template <typename ValueT, typename YieldAwaiterT>
        struct PromiseReturnValue
        {
            using value_type = ValueT;

            PromiseReturnValue() = default;

            template <typename U>
            void return_value(U&& v)
            {
                if constexpr (requires { _value.emplace(std::forward<U>(v)); })
                {
                    _value.emplace(std::forward<U>(v));
                }
                else if constexpr (requires { _value = std::forward<U>(v); })
                {
                    _value = std::forward<U>(v);
                }
                else
                {
                    // to support aggregate initialization
                    _value = decltype(_value){std::in_place, std::forward<U>(v)};
                }
            }

            template <typename U>
            auto yield_value(U&& v)
            {
                // save yield value in the same way,
                // as for normal return value.
                return_value(std::forward<U>(v));
                return YieldAwaiterT{};
            }

            auto&& ReturnValue() noexcept { return std::move(_value.value()); }

        private:
            std::optional<value_type> _value{};
        };

        template <typename YieldAwaiterT>
        struct PromiseReturnValue<void, YieldAwaiterT>
        {
            using value_type = void;

            void return_void() { }
        };

        template <typename ReturnerT>
        concept PromiseReturnerConcept = requires (ReturnerT r) {
            { r.return_void() };
        } || requires (ReturnerT r) {
            { r.return_value(std::declval<typename ReturnerT::value_type>()) };
            { r.yield_value(std::declval<typename ReturnerT::value_type>()) };
        };

        template <typename... Types>
        struct PromiseT;

        // IMPORTANT: The PromiseBase base class must be the first base class
        // from which the derived promise type inherits.
        // The reason is due to alignment within the coroutine frame.
        // The coroutine promise is laid out in memory according to the
        // most derived type, but if we have a base class like this,
        // it is only safe to use PromiseBase if it appears first
        // in the inheritance list.
        //
        // So with this PromiseBase, we can convert any derived promise
        // to std::coroutine_handle<PromiseBase>.
        // It is used inside the scheduler logic.
        template <concepts::IsAwaiter FinalAwaiterT, PromiseReturnerConcept ReturnerT, typename PauseHandlerT, typename StopSourceT>
        struct PromiseT<FinalAwaiterT, ReturnerT, PauseHandlerT, StopSourceT>
        : public PromiseBase<PROMISE_BASE_BUFFER_SIZE, FinalAwaiterT, PauseHandlerT, StopSourceT>, public ReturnerT
        {
            using PromiseBase_t = PromiseBase<PROMISE_BASE_BUFFER_SIZE, FinalAwaiterT, PauseHandlerT, StopSourceT>;

            auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }

            ~PromiseT() { this->Finish(); }
        };

    } // namespace detail

    template <typename ReturnValueT, typename PauseHandlerT = PauseHandler, typename StopSourceT = std::stop_source>
    using Promise = detail::PromiseT<detail::FinalAwaiter, detail::PromiseReturnValue<ReturnValueT, detail::FinalAwaiter>, PauseHandlerT, StopSourceT>;

    namespace detail {
        using CommonPromise = Promise<void>::PromiseBase_t;
    } // namespace detail

} // namespace tinycoro

#endif // TINY_CORO_PROMISE_NEW_HPP