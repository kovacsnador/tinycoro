#ifndef TINY_CORO_MUTEX_HPP
#define TINY_CORO_MUTEX_HPP

#include <atomic>

#include "PauseHandler.hpp"
#include "ReleaseGuard.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro {
    namespace detail {

        template <template <typename, typename> class AwaitableT>
        class Mutex
        {
        public:
            using awaitable_type = AwaitableT<Mutex, detail::PauseCallbackEvent>;

            friend class AwaitableT<Mutex, detail::PauseCallbackEvent>;
            friend class ReleaseGuard<Mutex>;

            Mutex() = default;

            // disable move and copy
            Mutex(Mutex&&) = delete;

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); }

            [[nodiscard]] auto Wait() noexcept { return awaitable_type{*this, detail::PauseCallbackEvent{}}; }

        private:
            // Checks only if the mutex is free to take,
            // and locks it if it was free before.
            bool Ready() noexcept
            {
                void* expected = nullptr;
                return _state.compare_exchange_strong(expected, this, std::memory_order_release, std::memory_order_relaxed);
            }

            void Release() noexcept
            {
                void* expected = _state.load(std::memory_order_relaxed);

                for (;;)
                {
                    // expected can NOT be nullptr
                    assert(expected != nullptr);
                    
                    void* wanted = nullptr;

                    if (expected != this)
                    {
                        // at this point expected
                        // can't be nullptr, because
                        // for sure somebody is holding
                        // the mutex.
                        auto oldHead = static_cast<awaitable_type*>(expected);
                        wanted       = oldHead->next ? static_cast<void*>(oldHead->next) : static_cast<void*>(this);
                    }

                    if (_state.compare_exchange_strong(expected, wanted, std::memory_order_release, std::memory_order_relaxed))
                    {
                        if (expected != this)
                        {
                            // if the expected (last set value)
                            // was not this, so some waiter from the list,
                            // we notify it for resumption.
                            static_cast<awaitable_type*>(expected)->Notify();
                        }

                        return;
                    }
                }
            }

            auto TryAcquire(awaitable_type* awaiter) noexcept
            {
                //void* expected = nullptr;
                auto expected = _state.load(std::memory_order_relaxed);

                for (;;)
                {
                    void* wanted = awaiter;

                    if (expected == nullptr)
                    {
                        // the mutex is free
                        wanted = this;
                    }
                    else if (expected == this)
                    {
                        // the mutex is locked,
                        // but there are no waiters
                        // in the stack.
                        awaiter->next = nullptr;
                    }
                    else
                    {
                        // the mutex is locked
                        // and we have already waiters
                        awaiter->next = static_cast<awaitable_type*>(expected);
                    }

                    if (_state.compare_exchange_strong(expected, wanted, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // success operation.
                        // we can exit from the loop.
                        break;
                    }
                }

                // if the last expected value is
                // a nullptr, that means that the
                // mutex was free to take,
                // so the aquire was a success.
                return expected == nullptr;
            }

            // nullptr => The mutex is free to take
            // this => Locked with NO waiters
            // other => Locked with waiters in the stack
            std::atomic<void*> _state{nullptr};
        };

        template <typename MutexT, typename EventT>
        class MutexAwaiter : public detail::SingleLinkable<MutexAwaiter<MutexT, EventT>>
        {
        public:
            MutexAwaiter(MutexT& mutex, EventT event)
            : _mutex{mutex}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            MutexAwaiter(MutexAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept { return _mutex.Ready(); }

            constexpr bool await_suspend(auto parentCoro) noexcept
            {
                PutOnPause(parentCoro);
                if (_mutex.TryAcquire(this))
                {
                    // no suspend, we held the mutex
                    ResumeFromPause(parentCoro);
                    return false;
                }

                // suspend, need to wait for the mutex
                return true;
            }

            [[nodiscard]] constexpr auto await_resume() noexcept { return ReleaseGuard{_mutex}; }

            void Notify() const noexcept { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

        private:
            MutexT& _mutex;
            EventT  _event;
        };

    } // namespace detail

    using Mutex = detail::Mutex<detail::MutexAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_MUTEX_HPP