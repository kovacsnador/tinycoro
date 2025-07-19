// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_PROMISE_NEW_HPP
#define TINY_CORO_PROMISE_NEW_HPP

#include <coroutine>
#include <future>

#include "AllocatorAdapter.hpp"
#include "PromiseSchedulable.hpp"
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
                    // reset the parent child,
                    // because we are done.
                    promise.parent->child = nullptr;

                    // We can strait jump and resume
                    // the parent coroutine.
                    return std::coroutine_handle<promise_t>::from_promise(*promise.parent);
                }

                return std::noop_coroutine();
            }

            void await_resume() const noexcept { }
        };

        template <typename... Args>
        struct PromiseReturnValue;

        // This is the default return value handler
        // It handles return values, which are
        // !is_trivially_constructible_v and !is_trivially_assignable_v
        // and are not references.
        template <typename ValueT, typename YieldAwaiterT>
        struct PromiseReturnValue<ValueT, YieldAwaiterT>
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

            decltype(auto) value() noexcept { return std::move(_value.value()); }

        private:
            std::optional<value_type> _value{};
        };

        template <typename ValueT, typename YieldAwaiterT>
            requires std::is_trivially_constructible_v<ValueT> && std::is_trivially_assignable_v<ValueT&, ValueT>
        struct PromiseReturnValue<ValueT, YieldAwaiterT>
        {
            using value_type = ValueT;

            PromiseReturnValue() = default;

            template <typename T>
            void return_value(T&& v)
            {
                if constexpr (requires { _value = std::forward<T>(v); })
                {
                    _value = std::forward<T>(v);
                }
                else if constexpr (requires { _value = value_type{std::forward<T>(v)}; })
                {
                    _value = value_type{std::forward<T>(v)};
                }
                else
                {
                    // special case for direct optional assignment
                    assert(v.has_value());
                    _value = v.value();
                }
            }

            template <typename T>
            auto yield_value(T&& v)
            {
                // save yield value in the same way,
                // as for normal return value.
                return_value(std::forward<T>(v));
                return YieldAwaiterT{};
            }

            value_type& value() noexcept { return _value; }

        private:
            ValueT _value{};
        };

        // Reference support for promise values
        //
        // In the inside value variable, we store
        // the reference as a simple pointer.
        template <typename ValueT, typename YieldAwaiterT>
        struct PromiseReturnValue<ValueT&, YieldAwaiterT>
        {
            using reference_t = ValueT&;
            using value_type  = ValueT;
            using pointer_t   = std::add_pointer_t<value_type>;

            PromiseReturnValue() = default;

            void return_value(reference_t v)
            {
                assert(!_value);
                _value = std::addressof(v);
            }

            auto yield_value(reference_t v)
            {
                // save yield value in the same way,
                // as for normal return value.
                return_value(v);
                return YieldAwaiterT{};
            }

            reference_t value() noexcept
            {
                assert(_value);
                return *std::exchange(_value, nullptr);
            }

        private:
            pointer_t _value{nullptr};
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

        // IMPORTANT: The SchedulablePromise base class must be the first base class
        // from which the derived promise type inherits.
        // The reason is due to alignment within the coroutine frame.
        // The coroutine promise is laid out in memory according to the
        // most derived type, but if we have a base class like this,
        // it is only safe to use SchedulablePromise if it appears first
        // in the inheritance list.
        //
        // So with this SchedulablePromise, we can convert any derived promise
        // to std::coroutine_handle<SchedulablePromise>.
        // It is used inside the scheduler logic.
        template <concepts::IsAwaiter    FinalAwaiterT,
                  PromiseReturnerConcept ReturnerT,
                  typename PauseHandlerT,
                  typename StopSourceT,
                  template <typename> class AllocatorT>
            requires concepts::IsAllocatorAdapter<AllocatorT>
        struct PromiseT : public SchedulablePromise<PROMISE_BASE_BUFFER_SIZE, FinalAwaiterT, PauseHandlerT, StopSourceT>,
                          public ReturnerT,
                          public AllocatorT<PromiseT<FinalAwaiterT, ReturnerT, PauseHandlerT, StopSourceT, AllocatorT>>
        {
            [[nodiscard]] auto get_return_object() noexcept { return std::coroutine_handle<PromiseT>::from_promise(*this); }

            // Here we trigger the base class Finish function.
            //
            // It is necessary, in order to notify
            // the waiters (scheduler, or other task)
            // that this coroutine is done.
            //
            // It's also important to mention,
            // that we can not rely on the base class
            // destructor here, becasue in Finish() we
            // need the value from the derived promise object.
            ~PromiseT() { 
                if(this->HasException() == false)
                {
                    // If there was no exception yet,
                    // trigger the Finish callback.
                    //
                    // Note:
                    // In case of an exception,
                    // Finish() is already triggered
                    // in SchedulableTask::Resume().
                    this->Finish({});
                }
            }
        };

        // Inline Promise class.
        // Only inherits from the promise base object.
        template <concepts::IsAwaiter    FinalAwaiterT,
                  PromiseReturnerConcept ReturnerT,
                  typename PauseHandlerT,
                  typename StopSourceT,
                  template <typename> class AllocatorT>
            requires concepts::IsAllocatorAdapter<AllocatorT>
        struct InlinePromiseT : public PromiseBase<FinalAwaiterT, PauseHandlerT, StopSourceT>,
                                public ReturnerT,
                                public AllocatorT<InlinePromiseT<FinalAwaiterT, ReturnerT, PauseHandlerT, StopSourceT, AllocatorT>>
        {
            [[nodiscard]] auto get_return_object() noexcept { return std::coroutine_handle<InlinePromiseT>::from_promise(*this); }
        };

        template <typename ReturnValueT,
                  template <typename> class AllocatorT = DefaultAllocator,
                  typename PauseHandlerT               = PauseHandler,
                  typename StopSourceT                 = std::stop_source>
        using Promise = detail::
            PromiseT<detail::FinalAwaiter, detail::PromiseReturnValue<ReturnValueT, detail::FinalAwaiter>, PauseHandlerT, StopSourceT, AllocatorT>;

        template <typename ReturnValueT,
                  template <typename> class AllocatorT = DefaultAllocator,
                  typename PauseHandlerT               = PauseHandler,
                  typename StopSourceT                 = std::stop_source>
        using InlinePromise = detail::InlinePromiseT<detail::FinalAwaiter,
                                                     detail::PromiseReturnValue<ReturnValueT, detail::FinalAwaiter>,
                                                     PauseHandlerT,
                                                     StopSourceT,
                                                     AllocatorT>;
    } // namespace detail

    namespace detail {
        // This represents a common schedulable promise type
        // that can be used for all Promise<T> specializations, (as base class)
        // regardless of their return type.
        using CommonSchedulablePromiseT = Promise<void>::PromiseBase_t;
    } // namespace detail

} // namespace tinycoro

#endif // TINY_CORO_PROMISE_NEW_HPP