#ifndef __TINY_CORO_MANUAL_EVENT_HPP__
#define __TINY_CORO_MANUAL_EVENT_HPP__

#include <atomic>

#include "PauseHandler.hpp"
#include "LinkedPtrStack.hpp"
#include "AwaiterHelper.hpp"

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

            void Reset() noexcept
            {
                typename decltype(_state)::value_type expected = this;
                _state.compare_exchange_strong(expected, nullptr);
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

            bool Cancel(awaiter_type* awaiter) noexcept
            {
                auto isAwaiter = [](auto* state, auto* self) { return (state && state != self); };

                detail::LinkedPtrStack<awaiter_type> elementsToNotify;
                
                bool erased{false};

                {
                    std::scoped_lock lock{_mtx};

                    auto expected = _state.load(std::memory_order::acquire);

                    // try to get the awaiter list
                    // out from the state
                    while (isAwaiter(expected, this) && !_state.compare_exchange_strong(expected, nullptr))
                    {
                    }

                    if (isAwaiter(expected, this))
                    {
                        // we have the awaiter list
                        // get the top element, and check
                        // if we still have our awaiter
                        // in the list
                        auto current = static_cast<awaiter_type*>(expected);

                        while (current != nullptr)
                        {
                            // iterate ovet the elemens
                            // and find our awaiter
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
                                    // Notify the awaiter
                                    elementsToNotify.push(current);
                                }
                            }
                            else
                            {
                                // we found our awaiter
                                erased = true;
                            }

                            // go to the next element
                            current = next;
                        }
                    }
                }

                // iterate over the elements
                // which needs to be notified.
                auto top = elementsToNotify.top();
                detail::IterInvoke(top, &awaiter_type::Notify);

                return erased;
            }

            // nullptr => NOT_SET and NO waiters
            // this => SET and NO waiters
            // other => NOT_SET with waiters
            std::atomic<void*> _state{nullptr};

            // mutex only used for cancellation
            std::mutex _mtx;
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

            ManualEventAwaiter* next{nullptr};

        private:
            ManualEventT&  _manualEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    using ManualEvent = detail::ManualEvent<detail::ManualEventAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_MANUAL_EVENT_HPP__