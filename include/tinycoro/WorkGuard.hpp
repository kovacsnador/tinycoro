#ifndef TINY_CORO_WORK_GUARD_HPP
#define TINY_CORO_WORK_GUARD_HPP

#include <functional>
#include <utility>
#include <atomic>

namespace tinycoro
{
    // A move-only RAII handle that keeps an `InlineScheduler` alive while work is
    // still expected to arrive.
    //
    // `InlineScheduler` is an alias of `detail::ConcurrentScheduler` and uses an
    // internal reference count to decide when `Run()` may exit. Constructing a
    // `WorkGuard` calls the scheduler's `_Acquire()`, while destroying the guard
    // or calling `Unlock()` releases that ownership exactly once via `_Release()`.
    //
    // Ownership may be transferred by move construction, move assignment, or
    // `swap()`. After a guard has been moved from or unlocked, it no longer keeps
    // the scheduler alive.
    //
    // This helper is intended for the inline scheduler only; the threaded
    // `ParallelScheduler` does not use work guards at the moment.
    template<typename SchedulerT>
    struct WorkGuard
    {
        WorkGuard() = default;
        
        explicit WorkGuard(SchedulerT& s)
        : _scheduler{std::addressof(s)}
        {
            s._Acquire();
        }

        WorkGuard(WorkGuard&& other) noexcept
        : _scheduler{other._scheduler.exchange(nullptr, std::memory_order::relaxed)}
        {
        }

        WorkGuard& operator=(WorkGuard&& other) noexcept
        {
            WorkGuard{std::move(other)}.swap(*this);
            return *this;
        }

        // Releases the owned scheduler reference, if any.
        //
        // \return `true` if this call released ownership, `false` if the guard was
        //         already empty.
        bool Unlock() noexcept
        {
            return _ExchangeUnlock(nullptr);
        }

        ~WorkGuard()
        {
            Unlock();
        }

        void swap(WorkGuard& other) noexcept
        {
            if (this == std::addressof(other))
                return;
        
            auto old = other._scheduler.exchange(
                _scheduler.exchange(nullptr, std::memory_order::relaxed),
                std::memory_order::relaxed);
            
            _ExchangeUnlock(old);
        }

    private:
        bool _ExchangeUnlock(SchedulerT* desired) noexcept
        {
            if(auto schedulerPtr = _scheduler.exchange(desired, std::memory_order::relaxed))
            {
                schedulerPtr->_Release();
                return true;
            }
            return false;
        }

        std::atomic<SchedulerT*> _scheduler{nullptr};
    };
    
} // namespace tinycoro

#endif // TINY_CORO_WORK_GUARD_HPP
