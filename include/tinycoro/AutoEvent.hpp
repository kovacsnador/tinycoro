// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_AUTO_EVENT_HPP
#define TINY_CORO_AUTO_EVENT_HPP

#include <atomic>
#include <mutex>

#include "PauseHandler.hpp"
#include "LinkedPtrStack.hpp"
#include "AwaiterHelper.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro {
    namespace detail {

        template <template <typename, typename> class AwaiterT>
        class AutoEvent
        {
        public:
            using awaiter_type = AwaiterT<AutoEvent, detail::PauseCallbackEvent>;

            friend class AwaiterT<AutoEvent, detail::PauseCallbackEvent>;

            AutoEvent(bool initialySet = false)
            : _state{initialySet ? this : nullptr}
            {
            }

            // disable move and copy
            AutoEvent(AutoEvent&&) = delete;

            void Set() noexcept
            {
                // It turned out that this Set() function
                // needs to use the mutex to prevent a race condition.
                //
                // The issue occurs if the expected awaiter has already
                // been notified and destroyed, but we still dereference it
                // to get the next element from it.
                std::unique_lock lock{_mtx};

                auto* expected = _state.load(std::memory_order_relaxed);
                void* desired  = nullptr;

                for (;;)
                {
                    if (expected == this)
                    {
                        // The event is already set
                        // so we can move on, nothing to do
                        return;
                    }

                    if (expected == nullptr)
                    {
                        // the event got unset,
                        // so we can try to set it
                        desired = this;
                    }
                    else
                    {
                        // the stack has changed so we can continue to pop
                        // the last value from it
                        //
                        // This line is the reason
                        // why we are using the mutex in this function.
                        //
                        // If we call Set() or Cancel() asynchronously,
                        // the "expected" variable can became a dangling pointer.
                        desired = static_cast<awaiter_type*>(expected)->next;
                    }

                    if (_state.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // unlock is here necessary,
                        // because after calling notify,
                        // it is not safe to release the mutex.
                        // error on MSVC: "unlock of unowned mutex"
                        lock.unlock();

                        // at this point expected can only be
                        // nullptr, or a valid awaiter but not this.
                        if (expected)
                        {
                            // finally we were able to pop an awaiter
                            // so we can notify it
                            static_cast<awaiter_type*>(expected)->Notify();
                        }

                        // operataion succeded.
                        return;
                    }
                }
            }

            bool IsSet() const noexcept
            {
                // checks if the current states points to this.
                // that means the event is set
                return _state.load(std::memory_order_acquire) == this;
            }

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); };

            [[nodiscard]] auto Wait() noexcept { return awaiter_type{*this, detail::PauseCallbackEvent{}}; };

        private:
            bool IsReady() noexcept
            {
                // try to reset the event, if succeeds that means
                // we can take the event.
                void* expected = this;
                return _state.compare_exchange_strong(expected, nullptr, std::memory_order_release, std::memory_order_relaxed);
            }

            bool Add(awaiter_type* awaiter)
            {
                auto* expected = _state.load(std::memory_order_relaxed);
                void* desired  = nullptr;

                for (;;)
                {
                    if (expected == this)
                    {
                        // the event got set,
                        // so we can try to get them.
                        desired = nullptr;

                        // safely reset the next pointer
                        // not strictly necessary but anyway
                        awaiter->next = nullptr;
                    }
                    else
                    {
                        // try to continue pushing to the front
                        // of the stack the awaiter
                        desired       = awaiter;
                        awaiter->next = static_cast<awaiter_type*>(expected);
                    }

                    if (_state.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // success operation
                        break;
                    }
                }

                // check the last state before a succesfull push,
                // if the last state was 'this' that means
                // we could just reset the event (to nullptr)
                // so the awaiter can be resumed without suspend
                return expected != this;
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
                            // we got exclusive access to
                            // the awaiter list
                            //
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
                                        // We geather all the awaiter
                                        // for a later point of a resumption.
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

            // nullptr => NOT set
            // this => Set no waiters
            // other => Not set with waiters
            std::atomic<void*> _state{nullptr};

            // It's used to protect
            // the awaiter list if we want to remove
            // from them. ( Set() or Cancel() )
            std::mutex _mtx;
        };

        template <typename AutoEventT, typename CallbackEventT>
        class AutoEventAwaiter : public detail::SingleLinkable<AutoEventAwaiter<AutoEventT, CallbackEventT>>
        {
        public:
            AutoEventAwaiter(AutoEventT& autoEvent, CallbackEventT event)
            : _autoEvent{autoEvent}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            AutoEventAwaiter(AutoEventAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                // check if already set the event.
                return _autoEvent.IsReady();
            }

            constexpr auto await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_autoEvent.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            constexpr auto await_resume() noexcept { }

            void Notify() const noexcept { _event.Notify(); }

            bool Cancel() noexcept { return _autoEvent.Cancel(this); }

        private:
            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            AutoEventT&    _autoEvent;
            CallbackEventT _event;
        };

    } // namespace detail

    using AutoEvent = detail::AutoEvent<detail::AutoEventAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_AUTO_EVENT_HPP