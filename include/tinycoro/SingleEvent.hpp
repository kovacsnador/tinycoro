#ifndef __TINY_CORO_SINGLE_EVENT_HPP__
#define __TINY_CORO_SINGLE_EVENT_HPP__

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
        struct SingleEvent
        {
            using value_type = ValueT;

            friend struct AwaiterT<SingleEvent, PauseCallbackEvent>;

            using awaiter_type = AwaiterT<SingleEvent, PauseCallbackEvent>;

            [[nodiscard]] auto operator co_await() { return awaiter_type{*this, PauseCallbackEvent{}}; }

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

            [[nodiscard]] bool HasValue(const awaiter_type* awaiter) noexcept
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

            bool Add(awaiter_type* awaiter, auto parentCoro)
            {
                std::scoped_lock lock{_mtx};
                if (_waiter)
                {
                    throw SingleEventException{"SingleEvent: Only 1 consumer allowed."};
                }

                // save the first awaiter
                _waiter = awaiter;

                if (_value.has_value() == false)
                {
                    // value is not set, put coroutine on pause
                    awaiter->PutOnPause(parentCoro);
                    return true;
                }

                // state is already set, we can continue with the coroutine.
                return false;
            }

            std::optional<ValueT> _value;

            const awaiter_type* _waiter{nullptr};
            mutable std::mutex  _mtx;
        };

        template <typename SingleEventT, typename CallbackEventT>
        struct SingleEventAwaiter
        {
            SingleEventAwaiter(SingleEventT& singleEvent, CallbackEventT event)
            : _singleEvent{singleEvent}
            , _event{std::move(event)}
            {
            }

            [[nodiscard]] constexpr bool await_ready() const noexcept
            {
                // check if already set the event.
                return _singleEvent.HasValue(this);
            }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                if (_singleEvent.Add(this, parentCoro) == false)
                {
                    // coroutine is not paused, we can continue immediately
                    return parentCoro;
                }
                return std::noop_coroutine();
            }

            // Moving out the value.
            [[nodiscard]] constexpr auto await_resume() noexcept {

                typename SingleEventT::value_type temp{std::move(_singleEvent._value.value())};    
                _singleEvent.Reset();
                return temp;
            }

            void Notify() const { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

        private:
            SingleEventT&  _singleEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    template <typename ValueT>
    using SingleEvent = detail::SingleEvent<ValueT, detail::SingleEventAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_SINGLE_EVENT_HPP__