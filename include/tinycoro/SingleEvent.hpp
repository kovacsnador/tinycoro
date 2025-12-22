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
#include "Common.hpp"

namespace tinycoro {

    namespace detail {

        // This is an auto reset event, with one consumer and one producer.
        template <concepts::NothrowMoveAssignable ValueT, template <typename, typename> class AwaiterT>
        class SingleEvent
        {
        public:
            using value_type = ValueT;

            friend class AwaiterT<SingleEvent, detail::ResumeSignalEvent>;

            using awaiter_type = AwaiterT<SingleEvent, detail::ResumeSignalEvent>;

            SingleEvent() = default;

            // disable move and copy
            SingleEvent(SingleEvent&&) = delete;

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); }

            [[nodiscard]] auto Wait() noexcept { return awaiter_type{*this, detail::ResumeSignalEvent{}}; }

            bool Set(ValueT val)
            {
                std::unique_lock lock{_mtx};

                if (_value.has_value())
                {
                    // we already set the value.
                    return false;
                }

                _value.emplace(std::move(val));

                if(auto waiter = std::exchange(_waiter, nullptr))
                {
                    waiter->SwapValue(_value);

                    assert(_waiter == nullptr);
                    assert(_value.has_value() == false);

                    lock.unlock();

                    waiter->Notify();
                }

                // value was set
                return true;
            }

            [[nodiscard]] bool IsSet() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return _value.has_value();
            }

        private:

            [[nodiscard]] bool IsReady(awaiter_type* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                assert(_waiter == nullptr);

                if (_value.has_value())
                {
                    awaiter->SwapValue(_value);
                    return true;
                }

                return false;
            }

            [[nodiscard]] bool Add(awaiter_type* awaiter)
            {
                std::scoped_lock lock{_mtx};
                if (_waiter)
                {
                    throw SingleEventException{"SingleEvent: Only 1 consumer allowed."};
                }

                if (_value.has_value())
                {
                    awaiter->SwapValue(_value);
                    return false;
                }

                // save the first awaiter
                _waiter = awaiter;
                return true;
            }

            [[nodiscard]] bool Cancel(awaiter_type* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};
                if (_waiter == awaiter)
                {
                    // reset the waiter
                    _waiter = nullptr;
                    return true;
                }
                return false;
            }

            [[nodiscard]] auto Exchange(awaiter_type* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                assert(_waiter);
                assert(_waiter == awaiter);
                //assert(_value.has_value());

                _waiter = nullptr;
                return std::exchange(_value, std::nullopt);
            }

            std::optional<ValueT> _value;

            awaiter_type* _waiter{nullptr};
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

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                // check if already set the event.
                return _singleEvent.IsReady(this);
            }

            [[nodiscard]] constexpr auto await_suspend(auto parentCoro)
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

            [[nodiscard]] constexpr auto await_resume() noexcept { return _value.value(); }

            bool Notify() const noexcept { return _event.Notify(ENotifyPolicy::RESUME); }

            bool NotifyToDestroy() const noexcept { return _event.Notify(ENotifyPolicy::DESTROY); }

            [[nodiscard]] bool Cancel() noexcept { return _singleEvent.Cancel(this); }

            template<typename T>
            void SwapValue(T& val) noexcept
            {   
                assert(_value.has_value() == false);
                std::swap(_value, val);
            }

        private:
            void PutOnPause(auto parentCoro) noexcept { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            SingleEventT&  _singleEvent;
            CallbackEventT _event;

            std::optional<typename SingleEventT::value_type> _value;
        };

    } // namespace detail

    template <typename ValueT>
    using SingleEvent = detail::SingleEvent<ValueT, detail::SingleEventAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_SINGLE_EVENT_HPP