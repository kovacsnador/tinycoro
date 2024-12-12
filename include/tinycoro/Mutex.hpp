#ifndef __TINY_CORO_MUTEX_HPP__
#define __TINY_CORO_MUTEX_HPP__

#include <atomic>

#include "PauseHandler.hpp"

namespace tinycoro {
    namespace detail {

        template <template <typename, typename> class AwaitableT>
        class Mutex
        {
        public:
            using awaitable_type = AwaitableT<Mutex, detail::PauseCallbackEvent>;

            friend class AwaitableT<Mutex, detail::PauseCallbackEvent>;

            Mutex() = default;

            // disable move and copy
            Mutex(Mutex&&) = delete;

            [[nodiscard]] auto operator co_await() { return awaitable_type{*this, detail::PauseCallbackEvent{}}; }

        private:
            void Release()
            {
                if(_control > 1)
                {
                    int k=0;
                    ++k;
                }
                
                auto oldValue = _state.load();

                assert(oldValue != nullptr);

                if (oldValue == this)
                {
                    if (_state.compare_exchange_strong(oldValue, nullptr))
                    {
                        --_control;
                        // No waiters, just release the mutex.
                        return;
                    }
                }

                auto oldHead = static_cast<awaitable_type*>(oldValue);

                void* next = oldHead->next ? static_cast<void*>(oldHead->next) : static_cast<void*>(this);

                while (_state.compare_exchange_strong(oldValue, next) == false)
                {
                    oldHead = static_cast<awaitable_type*>(oldValue);
                    next = oldHead->next ? static_cast<void*>(oldHead->next) : static_cast<void*>(this);

                }

                // wake the next awaiter
                oldHead->Notify();
            }

            auto TryAcquire(awaitable_type* awaiter)
            {
                auto oldValue = _state.load();

                if (oldValue == nullptr)
                {
                    if (_state.compare_exchange_strong(oldValue, this))
                    {
                        // mutex was free to take.
                        ++_control;
                        return true;
                    }
                }

                if (oldValue != this)
                {
                    awaiter->next = static_cast<awaitable_type*>(oldValue);
                }

                while (oldValue != nullptr && _state.compare_exchange_strong(oldValue, awaiter) == false)
                {
                    if (oldValue != this)
                    {
                        awaiter->next = static_cast<awaitable_type*>(oldValue);
                    }
                }

                if (oldValue == nullptr)
                {
                    // looks like the mutex is free to take, make a try...
                    return TryAcquire(awaiter);
                }

                // awaiter is now in waiting queue
                return false;
            }

            // nullptr => The mutex is free to take
            // this => Locked with NO waiters
            // other => Locked with waiters in the stack
            std::atomic<void*> _state{nullptr};

            std::atomic<size_t> _control{0};
        };

        template <typename MutexT, typename EventT>
        class MutexAwaiter
        {
        public:
            MutexAwaiter(MutexT& mutex, EventT event)
            : _mutex{mutex}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            MutexAwaiter(MutexAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_mutex.TryAcquire(this))
                {
                    // no suspend, we held the mutex
                    ResumeFromPause(parentCoro);
                    return parentCoro;
                }

                // suspend, need to wait for the mutex
                return std::noop_coroutine();
            }

            [[nodiscard]] constexpr auto await_resume() noexcept
            {
                return Finally([this] { _mutex.Release(); });
            }

            void Notify() const { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

            MutexAwaiter* next{nullptr};

        private:
            MutexT& _mutex;
            EventT  _event;
        };

    } // namespace detail

    using Mutex = detail::Mutex<detail::MutexAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_MUTEX_HPP__