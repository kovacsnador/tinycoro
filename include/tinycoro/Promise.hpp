#ifndef __TINY_CORO_PROMISE_HPP__
#define __TINY_CORO_PROMISE_HPP__

#include <type_traits>
#include <concepts>
#include <stop_token>

#include "tinycoro/PauseHandler.hpp"

namespace tinycoro {

    struct FinalAwaiter
    {
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        [[nodiscard]] std::coroutine_handle<> await_suspend(auto hdl) noexcept
        {
            auto& promise = hdl.promise();

            if (promise.parent)
            {
                promise.parent.Child().ReleaseHandle();
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

    template <typename ValueT, typename AwaiterT>
    struct PromiseYieldValue
    {
        using value_type = ValueT;

        template <typename U>
        auto yield_value(U&& v)
        {
            _value = std::forward<U>(v);
            return AwaiterT{};
        }

        const auto& YieldValue() const { return _value.value(); }

    private:
        std::optional<value_type> _value{};
    };

    template <typename PackedCoroHandleT>
    struct CoroHandleNode
    {
        PackedCoroHandleT parent;
        PackedCoroHandleT child;
    };

    template <typename FinalAwaiterT, concepts::PauseHandler PauseHandlerT = PauseHandler, typename StopSourceT = std::stop_source>
    struct PromiseBase : private CoroHandleNode<PackedCoroHandle>
    {
        PromiseBase()          = default;
        virtual ~PromiseBase() = default;

        PromiseBase(const PromiseBase&) = default;
        PromiseBase(PromiseBase&&)      = default;

        PromiseBase& operator=(const PromiseBase&) = default;
        PromiseBase& operator=(PromiseBase&&)      = default;

        using CoroHandleNode::child;
        using CoroHandleNode::parent;

        std::shared_ptr<PauseHandlerT> pauseHandler;

        StopSourceT stopSource;
        bool        cancellable{false};

        auto initial_suspend() { return std::suspend_always{}; }

        auto final_suspend() noexcept { return FinalAwaiterT{}; }

        void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
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

    template <typename FinalAwaiterT, PromiseReturnerConcept ReturnerT>
    struct PromiseT<FinalAwaiterT, ReturnerT> : public PromiseBase<FinalAwaiterT>, public ReturnerT
    {
        auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }
    };

    template <typename FinalAwaiterT, PromiseReturnerConcept ReturnerT, PromiseYielderConcept YielderT>
    struct PromiseT<FinalAwaiterT, ReturnerT, YielderT> : public PromiseBase<FinalAwaiterT>, public ReturnerT, public YielderT
    {
        auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }
    };

    template <typename ReturnValueT>
    using Promise = PromiseT<FinalAwaiter, PromiseReturnValue<ReturnValueT>>;

} // namespace tinycoro

#endif //!__TINY_CORO_PROMISE_HPP__