// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SINGLE_EVENT_HPP
#define TINY_CORO_SINGLE_EVENT_HPP

#include <optional>
#include <atomic>
#include <functional>
#include <mutex>
#include <coroutine>

#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace detail {

        // This is an auto reset event, with one consumer and one producer.
        template <typename ValueT, template <typename, typename> class AwaiterT>
        class SingleEvent
        {
        public:
            using value_type = ValueT;

            friend class AwaiterT<SingleEvent, detail::PauseCallbackEvent>;

            using awaiter_type = AwaiterT<SingleEvent, detail::PauseCallbackEvent>;

            SingleEvent() = default;

            // disable move and copy
            SingleEvent(SingleEvent&&) = delete;

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); }

            [[nodiscard]] auto Wait() noexcept { return awaiter_type{*this, detail::PauseCallbackEvent{}}; }

            void SetValue(ValueT val)
            {
                std::unique_lock lock{_mtx};

                if (_value.has_value())
                {
                    throw SingleEventException{"SingleEvent: Value is already set!"};
                }

                _value = std::move(val);

                if (_waiter)
                {
                    lock.unlock();
                    _waiter->Notify();
                }
            }

            [[nodiscard]] bool IsSet() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return _value.has_value();
            }

        private:
            void Reset()
            {
                std::scoped_lock lock{_mtx};
                _value.reset();
                _waiter = nullptr;
            }

            [[nodiscard]] bool IsReady(const awaiter_type* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                if (_value.has_value() && _waiter == nullptr)
                {
                    // save the first awaiter
                    _waiter = awaiter;

                    // coroutine is ready, we can continue
                    return true;
                }

                return false;
            }

            bool Add(awaiter_type* awaiter)
            {
                std::scoped_lock lock{_mtx};
                if (_waiter)
                {
                    throw SingleEventException{"SingleEvent: Only 1 consumer allowed."};
                }

                // save the first awaiter
                _waiter = awaiter;
                return !_value.has_value();
            }

            bool Cancel(const awaiter_type* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};
                if(_waiter == awaiter)
                {
                    // reset the waiter
                    _waiter = nullptr;
                    return true;
                }
                return false;
            }

            std::optional<ValueT> _value;

            const awaiter_type* _waiter{nullptr};
            mutable std::mutex  _mtx;
        };

        template <typename SingleEventT, typename CallbackEventT>
        class SingleEventAwaiter
        {
        public:
            SingleEventAwaiter(SingleEventT& singleEvent, CallbackEventT event)
            : _singleEvent{singleEvent}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            SingleEventAwaiter(SingleEventAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept
            {
                // check if already set the event.
                return _singleEvent.IsReady(this);
            }

            constexpr auto await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_singleEvent.Add(this) == false)
                {
                    // coroutine is not paused, we can continue immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            // Moving out the value.
            [[nodiscard]] constexpr auto await_resume() noexcept {

                typename SingleEventT::value_type temp{std::move(_singleEvent._value.value())};    
                _singleEvent.Reset();
                return temp;
            }

            void Notify() const noexcept { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            bool Cancel() noexcept { return _singleEvent.Cancel(this); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

        private:
            SingleEventT&  _singleEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    template <typename ValueT>
    using SingleEvent = detail::SingleEvent<ValueT, detail::SingleEventAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_SINGLE_EVENT_HPP