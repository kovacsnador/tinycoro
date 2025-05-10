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
                void* expected = _state.load();

                if (expected == this)
                {
                    // the event is already set,
                    // nothing to do
                    return;
                }

                void* desired = this;
                if (expected && expected != this)
                {
                    // we have waiter in the stack
                    desired = static_cast<awaiter_type*>(expected)->next;
                }

                while (!_state.compare_exchange_strong(expected, desired))
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
                        desired = static_cast<awaiter_type*>(expected)->next;
                    }
                }

                if (expected)
                {
                    // finally we were able to pop an awaiter
                    // so we can notify it
                    static_cast<awaiter_type*>(expected)->Notify();
                }
            }

            bool IsSet() const noexcept
            {
                // checks if the current states points to this.
                // that means the event is set
                return _state.load() == this;
            }

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); };

            [[nodiscard]] auto Wait() noexcept { return awaiter_type{*this, detail::PauseCallbackEvent{}}; };

        private:
            bool IsReady() noexcept
            {
                // try to reset the event, if succeeds that means
                // we can take the event.
                void* expected = this;
                return _state.compare_exchange_strong(expected, nullptr);
            }

            bool Add(awaiter_type* awaiter)
            {
                void* expected = _state.load();
                void* desired  = nullptr;

                if (expected != this)
                {
                    // we have some awaiters in the stack already
                    // or the stack is empty but,
                    // so we want to set the new awaiter on the
                    // front of the stack
                    desired       = awaiter;
                    awaiter->next = static_cast<awaiter_type*>(expected);
                }

                while (!_state.compare_exchange_strong(expected, desired))
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
                }

                // check the last state before a succesfull push
                // if the last state was 'this' that means
                // we could just reset the event (to nullptr)
                // so the awaiter can be resumed without suspend
                return expected != this;
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
                        // get the top element
                        auto current = static_cast<awaiter_type*>(expected);

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
                                    // Notify the awaiter
                                    elementsToNotify.push(current);
                                }
                            }
                            else
                            {
                                // we found our awaiter,
                                erased = true;
                            }

                            // jump to the next element
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

            // nullptr => NOT set
            // this => Set no waiters
            // other => Not set with waiters
            std::atomic<void*> _state{nullptr};

            // only used for awaiter cancellation
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