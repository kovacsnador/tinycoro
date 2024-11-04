#ifndef __TINY_CORO_MANUAL_EVENT_HPP__
#define __TINY_CORO_MANUAL_EVENT_HPP__

#include <atomic>

#include "PauseHandler.hpp"

namespace tinycoro {

    namespace detail {

        template <template <typename, typename> class AwaiterT>
        class ManualEvent
        {
        public:
            using awaiter_type = AwaiterT<ManualEvent, PauseCallbackEvent>;

            friend class AwaiterT<ManualEvent, PauseCallbackEvent>;

            ManualEvent() = default;

            // disable move and copy
            ManualEvent(ManualEvent&&) = delete;

            void Set() noexcept
            {
                auto oldValue = _state.exchange(this);
                if (oldValue != this && oldValue)
                {
                    // handle awaiters
                    for (auto* top = static_cast<awaiter_type*>(oldValue); top;)
                    {
                        auto next = top->next;
                        top->Notify();
                        top = next;
                    }
                }
            }

            bool IsSet() const noexcept { return _state.load() == this; }

            void Reset() noexcept
            {
                typename decltype(_state)::value_type expected = this;
                _state.compare_exchange_strong(expected, nullptr);
            }

            auto operator co_await() noexcept { return awaiter_type{*this, PauseCallbackEvent{}}; };

        private:
            bool Add(awaiter_type* awaiter)
            {
                auto oldValue = _state.load(std::memory_order::acquire);
                if (oldValue != this)
                {
                    awaiter->next = static_cast<awaiter_type*>(oldValue);
                    while (_state.compare_exchange_strong(oldValue, awaiter) == false)
                    {
                        if (oldValue == this)
                        {
                            // continue without suspension
                            return false;
                        }
                        awaiter->next = static_cast<awaiter_type*>(oldValue);
                    }
                    // suspend the coroutine
                    return true;
                }
                // continue without suspension
                return false;
            }

            // nullptr => NOT_SET and NO waiters
            // this => SET and NO waiters
            // other => NOT_SET with waiters
            std::atomic<void*> _state{nullptr};
        };

        template <typename ManualEventT, typename CallbackEventT>
        class ManualEventAwaiter
        {
        public:
            ManualEventAwaiter(ManualEventT& manualEvent, CallbackEventT event)
            : _manualEvent{manualEvent}
            , _event{std::move(event)}
            {
            }

            [[nodiscard]] constexpr bool await_ready() const noexcept
            {
                // check if already set the event.
                return _manualEvent.IsSet();
            }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_manualEvent.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return parentCoro;
                }
                return std::noop_coroutine();
            }

            constexpr auto await_resume() noexcept { }

            void Notify() const { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

            ManualEventAwaiter* next{nullptr};

        private:
            ManualEventT&  _manualEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    using ManualEvent = detail::ManualEvent<detail::ManualEventAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_MANUAL_EVENT_HPP__