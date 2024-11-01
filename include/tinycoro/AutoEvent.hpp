#ifndef __TINY_CORO_AUTO_EVENT_HPP__
#define __TINY_CORO_AUTO_EVENT_HPP__

#include <mutex>

#include "PauseHandler.hpp"
#include "LinkedPtrStack.hpp"

namespace tinycoro {
    namespace detail {

        template <template <typename, typename> class AwaiterT>
        struct AutoEvent
        {
            using awaiter_type = AwaiterT<AutoEvent, PauseCallbackEvent>;

            friend struct AwaiterT<AutoEvent, PauseCallbackEvent>;

            AutoEvent(bool initialySet = false)
            : _state{initialySet}
            {
            }

            AutoEvent(AutoEvent&&) = delete;

            void Set() noexcept
            {
                std::unique_lock lock{_mtx};

                if (_state == false)
                {
                    if (auto* top = _waiters.pop())
                    {
                        lock.unlock();

                        top->Notify();
                        return;
                    }
                }
                _state = true;
            }

            auto operator co_await() noexcept { return awaiter_type{*this, PauseCallbackEvent{}}; };

        private:
            bool IsReady() noexcept
            {
                std::unique_lock lock{_mtx};

                if (_state)
                {
                    _state = false;
                    return true;
                }
                return false;
            }

            bool Add(awaiter_type* awaiter)
            {
                std::scoped_lock lock{_mtx};

                // is already set
                if (_state)
                {
                    _state = false;
                    return false;
                }

                // Not set
                _waiters.push(awaiter);
                return true;
            }

            bool                         _state;
            LinkedPtrStack<awaiter_type> _waiters;
            std::mutex                   _mtx;
        };

        template <typename AutoEventT, typename CallbackEventT>
        struct AutoEventAwaiter
        {
            AutoEventAwaiter(AutoEventT& autoEvent, CallbackEventT event)
            : _autoEvent{autoEvent}
            , _event{std::move(event)}
            {
            }

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                // check if already set the event.
                return _autoEvent.IsReady();
            }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_autoEvent.Add(this) == false)
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

            AutoEventAwaiter* next{nullptr};

        private:
            AutoEventT&    _autoEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    using AutoEvent = detail::AutoEvent<detail::AutoEventAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_AUTO_EVENT_HPP__