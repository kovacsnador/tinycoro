#ifndef __TINY_CORO_SINGLE_CONSUMER_EVENT_HPP__
#define __TINY_CORO_SINGLE_CONSUMER_EVENT_HPP__

#include <optional>
#include <atomic>
#include <functional>
#include <mutex>
#include <coroutine>

#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {

    template <typename ValueT, template <typename, typename> class AwaiterT>
    struct SingleEventT
    {
        friend struct AwaiterT<SingleEventT, PauseCallbackEvent>;

        using awaiter_type = AwaiterT<SingleEventT, PauseCallbackEvent>;

        [[nodiscard]] auto operator co_await() { return awaiter_type{*this, PauseCallbackEvent{}}; }

        template <typename T>
        void SetValue(T&& val)
        {
            if (_state.load())
            {
                throw SingleEventException{"Value is already set!"};
            }

            std::unique_lock lock{_mtx};
            _value = std::forward<T>(val);
            _state.store(true, std::memory_order::release);

            if (_waiter)
            {
                lock.unlock();
                _waiter->Notify();
            }
        }

    private:
        [[nodiscard]] bool HasValue() const noexcept { return _state.load(); }

        bool Add(awaiter_type* awaiter, auto parentCoro)
        {
            std::unique_lock lock{_mtx};
            if (_waiter)
            {
                throw SingleEventException{"Only 1 consumer allowed."};
            }

            if (_state.load() == false)
            {
                // value is not set, save awaiter for later notify
                awaiter->PutOnPause(parentCoro);
                _waiter = awaiter;

                return true;
            }

            // state is already set, we can continue with the coroutine.
            return false;
        }

        std::optional<ValueT> _value;
        std::atomic<bool>     _state{false};

        awaiter_type* _waiter{nullptr};
        std::mutex    _mtx;
    };

    namespace detail {

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
                return _singleEvent.HasValue();
            }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                if (_singleEvent.Add(this, parentCoro) == false)
                {
                    return parentCoro;
                }
                return std::noop_coroutine();
            }

            // Moving out the value. 
            [[nodiscard]] constexpr auto&& await_resume() noexcept { return std::move(_singleEvent._value.value()); }

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
    using SingleEvent = SingleEventT<ValueT, detail::SingleEventAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_SINGLE_CONSUMER_EVENT_HPP__