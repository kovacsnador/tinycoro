#ifndef TINY_CORO_WORK_GUARD_HPP
#define TINY_CORO_WORK_GUARD_HPP

#include <functional>
#include <utility>

namespace tinycoro
{
    /// A RAII helper used to keep an inline scheduler alive while work is outstanding.
    ///
    /// `InlineScheduler` (a.k.a. `detail::ConcurrentScheduler`) uses a work guard
    /// reference count to determine when it can exit its `Run()` loop. `MakeWorkGuard`
    /// increments the scheduler's internal counter on creation and decrements it on
    /// destruction.
    ///
    /// This helper is intentionally lightweight and is intended for use with the
    /// inline scheduler (no support for the threaded `ParallelScheduler`).
    struct WorkGuard
    {
        using callback_t = std::function<void()>;

        WorkGuard() = default;

        explicit WorkGuard(callback_t cb)
        : _release{std::move(cb)}
        {
        }

        WorkGuard(WorkGuard&& other) noexcept
        : _release{std::exchange(other._release, nullptr)}
        {
        }

        WorkGuard& operator=(WorkGuard&& other) noexcept
        {
            WorkGuard{std::move(other)}.swap(*this);
            return *this;
        }

        void Unlock() noexcept
        {
            if(_release)
            {
                _release();
                _release = nullptr;
            }
        }

        ~WorkGuard()
        {
            Unlock();
        }

        void swap(WorkGuard& other) noexcept
        {
            std::swap(_release, other._release);
        }

    private:
        callback_t _release{nullptr};
    };

    template<typename SchedulerT>
    static WorkGuard MakeWorkGuard(SchedulerT& scheduler) noexcept
    {
        scheduler._Acquire();

        return WorkGuard{[p = std::addressof(scheduler)] { 
            assert(p);
            p->_Release();
        }};
    }
    
} // namespace tinycoro

#endif // TINY_CORO_WORK_GUARD_HPP