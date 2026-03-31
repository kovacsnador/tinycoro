#ifndef TINY_CORO_WORK_GUARD_HPP
#define TINY_CORO_WORK_GUARD_HPP

#include <functional>
#include <utility>
#include <atomic>

namespace tinycoro
{
    // A small RAII handle that keeps a guarded object alive while work is still
    // expected to arrive.
    //
    // The guarded type must provide `_Acquire()` and `_Release()` members.
    // Constructing a `WorkGuard` acquires one ownership reference by calling
    // `_Acquire()`. Destroying the guard, or calling `Unlock()`, releases that
    // ownership exactly once via `_Release()`.
    //
    // `WorkGuard` is copyable and moveable:
    // - copying acquires an additional ownership reference,
    // - moving transfers the existing ownership without touching the counter,
    // - `swap()` exchanges ownership between guards.
    //
    // After a guard has been moved from or unlocked, it becomes empty and no
    // longer keeps the guarded object alive.
    //
    // Typical guarded objects are `InlineScheduler` and `TaskGroup`.
    template<typename GuardedT>
    struct WorkGuard
    {
        WorkGuard() = default;
        
        explicit WorkGuard(GuardedT& s)
        : _guarded{std::addressof(s)}
        {
            s._Acquire();
        }

        WorkGuard(const WorkGuard& other)
        {
            if(auto guarded = other._guarded.load(std::memory_order::relaxed))
            {
                guarded->_Acquire();
                _guarded.store(guarded, std::memory_order::relaxed);
            }
        }

        WorkGuard(WorkGuard&& other) noexcept
        : _guarded{other._guarded.exchange(nullptr, std::memory_order::relaxed)}
        {
        }

        WorkGuard& operator=(const WorkGuard& other)
        {
            WorkGuard{other}.swap(*this);
            return *this;
        }

        WorkGuard& operator=(WorkGuard&& other) noexcept
        {
            WorkGuard{std::move(other)}.swap(*this);
            return *this;
        }

        // Releases the owned reference, if any.
        //
        // \return `true` if this call released ownership, `false` if the guard was
        //         already empty.
        bool Unlock() noexcept
        {
            return _ExchangeUnlock(nullptr);
        }

        // Returns `true` if this guard currently owns a reference.
        [[nodiscard]] bool Owner() const noexcept { return _guarded.load(std::memory_order::relaxed); }

        ~WorkGuard()
        {
            Unlock();
        }

        void swap(WorkGuard& other) noexcept
        {
            if (this == std::addressof(other))
                return;
        
            auto old = other._guarded.exchange(
                _guarded.exchange(nullptr, std::memory_order::relaxed),
                std::memory_order::relaxed);
            
            _ExchangeUnlock(old);
        }

    private:
        bool _ExchangeUnlock(GuardedT* desired) noexcept
        {
            if(auto schedulerPtr = _guarded.exchange(desired, std::memory_order::relaxed))
            {
                schedulerPtr->_Release();
                return true;
            }
            return false;
        }

        std::atomic<GuardedT*> _guarded{nullptr};
    };
    
} // namespace tinycoro

#endif // TINY_CORO_WORK_GUARD_HPP
