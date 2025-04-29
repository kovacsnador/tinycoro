#ifndef TINY_CORO_PROMISE_HPP
#define TINY_CORO_PROMISE_HPP

#include <type_traits>
#include <concepts>
#include <stop_token>

#include "PauseHandler.hpp"
#include "PackedCoroHandle.hpp"
#include "IntrusivePtr.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept Awaiter = requires (T t) {
            { t.await_ready() };
            { t.await_suspend(std::coroutine_handle<>{}) };
            { t.await_resume() };
        };

    } // namespace concepts

    struct FinalAwaiter
    {
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        [[nodiscard]] std::coroutine_handle<> await_suspend(auto hdl) noexcept
        {
            auto& promise = hdl.promise();

            if (promise.parent)
            {
                promise.parent.Child() = nullptr;
                return promise.parent.Handle();
            }

            return std::noop_coroutine();
        }

        void await_resume() const noexcept { }
    };

    template <typename ValueT>
    struct PromiseReturnValue
    {
        using value_type = ValueT;

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

    template <typename ValueT, concepts::Awaiter AwaiterT>
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

    template <concepts::Awaiter FinalAwaiterT, concepts::PauseHandler PauseHandlerT, typename StopSourceT, typename NodeT = PackedCoroHandle>
    struct PromiseBase
    {
        PromiseBase() = default;

        // disable nove and copy
        PromiseBase(PromiseBase&&) = delete;

        NodeT child;
        NodeT parent;

        detail::IntrusivePtr<PauseHandlerT> pauseHandler;

        // at the beginning we not initialize
        // the stop source here, the initialization
        // will be delayed, until we actually need this object
        StopSourceT stopSource{std::nostopstate};

        auto initial_suspend() { return std::suspend_always{}; }

        auto final_suspend() noexcept { return FinalAwaiterT{}; }

        [[noreturn]] constexpr void unhandled_exception() const { std::rethrow_exception(std::current_exception()); }

        template <typename... Args>
        auto MakePauseHandler(Args&&... args)
        {
            pauseHandler.emplace(std::forward<Args>(args)...);
            return pauseHandler.get();
        }

        // Getting the corresponding stop source,
        // and make sure it is initialized
        auto& StopSource() noexcept
        {
            if (stopSource.stop_possible() == false)
            {
                // initialize stop source
                // if it has no state yet
                stopSource = {};
            }
            return stopSource;
        }
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

    template <concepts::Awaiter FinalAwaiterT, PromiseReturnerConcept ReturnerT, typename PauseHandlerT, typename StopSourceT>
    struct PromiseT<FinalAwaiterT, ReturnerT, PauseHandlerT, StopSourceT> : public PromiseBase<FinalAwaiterT, PauseHandlerT, StopSourceT>,
                                                                            public ReturnerT
    {
        auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }
    };

    template <concepts::Awaiter      FinalAwaiterT,
              PromiseReturnerConcept ReturnerT,
              PromiseYielderConcept  YielderT,
              typename PauseHandlerT,
              typename StopSourceT>
    struct PromiseT<FinalAwaiterT, ReturnerT, YielderT, PauseHandlerT, StopSourceT>
    : public PromiseBase<FinalAwaiterT, PauseHandlerT, StopSourceT>, public ReturnerT, public YielderT
    {
        auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }
    };

    template <typename ReturnValueT, typename PauseHandlerT = PauseHandler, typename StopSourceT = std::stop_source>
    using Promise = PromiseT<FinalAwaiter, PromiseReturnValue<ReturnValueT>, PauseHandlerT, StopSourceT>;

} // namespace tinycoro

#endif // TINY_CORO_PROMISE_HPP