// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SEMAPHORE_HPP
#define TINY_CORO_SEMAPHORE_HPP

#include <mutex>
#include <coroutine>

#include "ResumeSignalEvent.hpp"
#include "LinkedPtrQueue.hpp"
#include "ReleaseGuard.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro {

    namespace detail {

        template <size_t LeastMaxValue, template <typename, typename> class AwaitableT, template <typename> class QueueT>
        class Semaphore
        {
            static_assert(LeastMaxValue > 0, "Semaphore LeastMaxValue needs to be bigger than 0");

        public:
            constexpr static size_t max = LeastMaxValue;

            using awaitable_type = AwaitableT<Semaphore, detail::ResumeSignalEvent>;

            friend class AwaitableT<Semaphore, detail::ResumeSignalEvent>;

            Semaphore(size_t initCount = LeastMaxValue)
            : _counter{std::min(LeastMaxValue, initCount)}
            {
                assert(initCount <= LeastMaxValue);
            }

            // disable move and copy
            Semaphore(Semaphore&&) = delete;

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); }

            [[nodiscard]] auto Wait() noexcept { return awaitable_type{*this, detail::ResumeSignalEvent{}}; }

            void Release(size_t count = 1) noexcept
            {
                for (; count > 0; --count)
                {
                    if (std::unique_lock lock{_mtx}; auto topAwaiter = _waiters.pop())
                    {
                        lock.unlock();

                        // Wake up the awaiter for resumption
                        topAwaiter->Notify();
                    }
                    else
                    {
                        assert(lock.owns_lock());

                        auto old = _counter.load(std::memory_order::acquire);
                        while(old < LeastMaxValue)
                        {
                            auto desired = std::min(old + count, LeastMaxValue);
                            if (_counter.compare_exchange_strong(old, desired, std::memory_order::release, std::memory_order::relaxed))
                            {
                                // we notify all waiters here.
                                // in case we release more than one free spot
                                // at the same time. 
                                // e.g (count > 1)
                                _counter.notify_all();
                                return;
                            }
                        }
                    }
                }
            }

            // This Acquire is blocking
            void Acquire() noexcept
            {
                for (;;)
                {
                    if (TryAcquire())
                    {
                        // we got the semaphore
                        return;
                    }

                    // TryAcquire is failed (_counter was zero at some point), so we need
                    // to wait for a _counter to be increased again.
                    _counter.wait(0);
                }
            }

            // Nonblocking try acquire
            [[nodiscard]] bool TryAcquire() noexcept
            {
                auto old = _counter.load(std::memory_order::acquire);
                while (old > 0)
                {
                    if (_counter.compare_exchange_strong(old, old - 1, std::memory_order::release, std::memory_order::relaxed))
                    {
                        // we got the semaphore
                        return true;
                    }
                }
                return false;
            }

            // Get the least maximum value as counter from the semaphore
            static constexpr auto Max() noexcept { return LeastMaxValue; };

        private:
            [[nodiscard]] auto _TryAcquire(awaitable_type* awaiter, auto parentCoro) noexcept
            {
                // needs to held the lock here
                // because somebody may called release...
                std::scoped_lock lock{_mtx};

                if (TryAcquire())
                {
                    // we got the semaphore
                    return true;
                }
                
                // we need to wait for the semaphore
                awaiter->PutOnPause(parentCoro);
                _waiters.push(awaiter);
           
                return false;
            }

            [[nodiscard]] bool _Cancel(awaitable_type* awaiter) noexcept
            { 
                std::scoped_lock lock{_mtx};

                // try to remove the awaiter from the list.
                return _waiters.erase(awaiter);
            }

            std::atomic<size_t>    _counter;
            QueueT<awaitable_type> _waiters;
            std::mutex             _mtx;
        };

        template <typename SemaphoreT, typename EventT>
        class SemaphoreAwaiter : public detail::SingleLinkable<SemaphoreAwaiter<SemaphoreT, EventT>>
        {
        public:
            SemaphoreAwaiter(SemaphoreT& semaphore, EventT event)
            : _semaphore{semaphore}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            SemaphoreAwaiter(SemaphoreAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept { return _semaphore.TryAcquire(); }

            [[nodiscard]] constexpr auto await_suspend(auto parentCoro) noexcept { return !_semaphore._TryAcquire(this, parentCoro); }

            [[nodiscard]] constexpr auto await_resume() noexcept
            {
                return detail::ReleaseGuardRelaxedImpl<ReleaseGuard, std::remove_cvref_t<decltype(_semaphore)>>{_semaphore};
            }

            bool Notify() const noexcept { return _event.Notify(ENotifyPolicy::RESUME); }

            bool NotifyToDestroy() const noexcept { return _event.Notify(ENotifyPolicy::DESTROY); }

            [[nodiscard]] bool Cancel() noexcept { return _semaphore._Cancel(this); }

            void PutOnPause(auto parentCoro) noexcept { _event.Set(context::PauseTask(parentCoro)); }

        private:
            SemaphoreT& _semaphore;
            EventT      _event;
        };

    } // namespace detail

    // counting semaphore
    template <auto LeastMaxValue>
    using Semaphore = detail::Semaphore<LeastMaxValue, detail::SemaphoreAwaiter, detail::LinkedPtrQueue>;

    // binary semaphore
    using BinarySemaphore = detail::Semaphore<1, detail::SemaphoreAwaiter, detail::LinkedPtrQueue>;

} // namespace tinycoro

#endif // TINY_CORO_SEMAPHORE_HPP