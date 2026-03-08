#ifndef TINY_CORO_WORK_GUARD_HPP
#define TINY_CORO_WORK_GUARD_HPP

#include <functional>
#include <utility>

namespace tinycoro
{
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

        constexpr void swap(WorkGuard& other) noexcept
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