#ifndef TINY_CORO_MANUAL_EVENT_HPP
#define TINY_CORO_MANUAL_EVENT_HPP

#include <atomic>

#include "PauseHandler.hpp"
#include "LinkedPtrStack.hpp"
#include "AwaiterHelper.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro {

    namespace detail {

        template <template <typename, typename> class AwaiterT>
        class ManualEvent
        {
        public:
            using awaiter_type = AwaiterT<ManualEvent, detail::PauseCallbackEvent>;

            friend class AwaiterT<ManualEvent, detail::PauseCallbackEvent>;

            ManualEvent(bool preSet = false)
            {
                if (preSet)
                {
                    Set();
                }
            }

            // disable move and copy
            ManualEvent(ManualEvent&&) = delete;

            void Set() noexcept
            {
                auto oldValue = _state.exchange(this);
                if (oldValue != this && oldValue)
                {
                    // handle awaiters
                    auto top = static_cast<awaiter_type*>(oldValue);
                    detail::IterInvoke(top, &awaiter_type::Notify);
                }
            }

            [[nodiscard]] bool IsSet() const noexcept { return _state.load() == this; }

            bool Reset() noexcept
            {
                typename decltype(_state)::value_type expected = this;
                return _state.compare_exchange_strong(expected, nullptr, std::memory_order_release, std::memory_order_relaxed);
            }

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); };

            [[nodiscard]] auto Wait() noexcept { return awaiter_type{*this, detail::PauseCallbackEvent{}}; };

        private:
            [[nodiscard]] bool Add(awaiter_type* awaiter)
            {
                auto oldValue = _state.load(std::memory_order::acquire);
                if (oldValue != this)
                {
                    awaiter->next = static_cast<awaiter_type*>(oldValue);
                    while (_state.compare_exchange_strong(oldValue, awaiter, std::memory_order_release, std::memory_order_relaxed) == false)
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

            bool Cancel(awaiter_type* awaiter) noexcept
            {
                auto isAwaiter = [](auto* state, auto* self) { return (state && state != self); };

                // In this stack, we collect all the
                // awaiters that need to be notified.
                //
                // The reason is to hold the mutex
                // for as short a time as possible.
                detail::LinkedPtrStack<awaiter_type> elementsToNotify;

                bool cancelled{false};

                {
                    std::scoped_lock lock{_mtx};

                    auto expected = _state.load(std::memory_order_relaxed);

                    while (isAwaiter(expected, this))
                    {
                        if (_state.compare_exchange_strong(expected, nullptr, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // we have the awaiter list
                            // get the top element
                            auto current = static_cast<awaiter_type*>(expected);

                            // iterate over the elements and
                            // search for the awaiter which we want to cancel
                            while (current != nullptr)
                            {
                                auto next = current->next;

                                if (current != awaiter)
                                {
                                    // if this is not our awaiter
                                    // push back into the queue
                                    if (Add(current) == false)
                                    {
                                        // we were not able to
                                        // register it again
                                        // so there is no suspension..
                                        //
                                        // collect the awaiter
                                        elementsToNotify.push(current);
                                    }
                                }
                                else
                                {
                                    // We found our awaiter,
                                    // we simply just skipping it.
                                    cancelled = true;
                                }

                                // jump to the next element
                                current = next;
                            }

                            // exit from the loop
                            break;
                        }
                    }
                }

                // iterate over the elements
                // which needs to be notified.
                auto top = elementsToNotify.top();
                detail::IterInvoke(top, &awaiter_type::Notify);

                return cancelled;
            }

            // nullptr => NOT_SET and NO waiters
            // this => SET and NO waiters
            // other => NOT_SET with waiters
            std::atomic<void*> _state{nullptr};

            // It's used to protect
            // the awaiter list if we want to remove
            // from them. ( see Cancel() )
            std::mutex _mtx;
        };

        template <typename ManualEventT, typename CallbackEventT>
        class ManualEventAwaiter : public detail::SingleLinkable<ManualEventAwaiter<ManualEventT, CallbackEventT>>
        {
        public:
            ManualEventAwaiter(ManualEventT& manualEvent, CallbackEventT event)
            : _manualEvent{manualEvent}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            ManualEventAwaiter(ManualEventAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept
            {
                // check if already set the event.
                return _manualEvent.IsSet();
            }

            constexpr auto await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_manualEvent.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            constexpr auto await_resume() noexcept { }

            void Notify() const noexcept { _event.Notify(); }

            bool Cancel() noexcept { return _manualEvent.Cancel(this); }

            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

        private:
            ManualEventT&  _manualEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    using ManualEvent = detail::ManualEvent<detail::ManualEventAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_MANUAL_EVENT_HPP