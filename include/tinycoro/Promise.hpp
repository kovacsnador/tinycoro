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

        template <typename ValueT>
        struct PromiseReturnValue
        {
            using value_type = ValueT;

            PromiseReturnValue() = default;

            template <typename U>
            void return_value(U&& v)
            {
                if constexpr (requires { _value = std::forward<U>(v); })
                {
                    _value = std::forward<U>(v);
                }
                else
                {
                    // to support aggregate initialization
                    _value = decltype(_value){std::in_place, std::forward<U>(v)};
                }
            }

            auto&& ReturnValue() { return std::move(_value.value()); }

        private:
            std::optional<value_type> _value{};
        };

        template <>
        struct PromiseReturnValue<void>
        {
            using value_type = void;

            void return_void() { }
        };

        template <typename ValueT, concepts::IsAwaiter AwaiterT>
        struct PromiseYieldValue
        {
            using value_type = ValueT;

            template <typename U>
            auto yield_value(U&& v)
            {
                _value.emplace(std::forward<U>(v));
                return AwaiterT{};
            }

            const auto& YieldValue() const { return _value.value(); }

        private:
            std::optional<value_type> _value{};
        };

        template <typename ReturnerT>
        concept PromiseReturnerConcept = requires (ReturnerT r) {
            { r.return_void() };
        } || requires (ReturnerT r) {
            { r.return_value(std::declval<typename ReturnerT::value_type>()) };
        };

        template <typename YielderT>
        concept PromiseYielderConcept = requires (YielderT y) {
            { y.yield_value(std::declval<typename YielderT::value_type>()) };
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

        template <concepts::IsAwaiter    FinalAwaiterT,
                  PromiseReturnerConcept ReturnerT,
                  PromiseYielderConcept  YielderT,
                  typename PauseHandlerT,
                  typename StopSourceT>
        struct PromiseT<FinalAwaiterT, ReturnerT, YielderT, PauseHandlerT, StopSourceT>
        : public PromiseBase<PROMISE_BASE_BUFFER_SIZE, FinalAwaiterT, PauseHandlerT, StopSourceT>, public ReturnerT, public YielderT
        {
            using PromiseBase_t = PromiseBase<PROMISE_BASE_BUFFER_SIZE, FinalAwaiterT, PauseHandlerT, StopSourceT>;

            auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }

            ~PromiseT() { this->Finish(); }
        };

    } // namespace detail

    template <typename ReturnValueT, typename PauseHandlerT = PauseHandler, typename StopSourceT = std::stop_source>
    using Promise = detail::PromiseT<detail::FinalAwaiter, detail::PromiseReturnValue<ReturnValueT>, PauseHandlerT, StopSourceT>;

    namespace detail {
        using CommonPromise = Promise<void>::PromiseBase_t;
    } // namespace detail

} // namespace tinycoro

#endif // TINY_CORO_PROMISE_NEW_HPP