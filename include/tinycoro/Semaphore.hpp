#ifndef __TINY_CORO_SEMAPHORE_H__
#define __TINY_CORO_SEMAPHORE_H__

#include <mutex>
#include <coroutine>
#include <stdexcept>

#include "PauseHandler.hpp"
#include "Finally.hpp"
#include "LinkedPtrStack.hpp"

namespace tinycoro {

    struct SemaphoreException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    namespace detail {

        template <template <typename, typename> class AwaitableT, typename EventT, template <typename> class StackT>
        class SemaphoreType
        {
        public:
            using awaitable_type = AwaitableT<EventT, SemaphoreType>;

            friend class AwaitableT<EventT, SemaphoreType>;

            SemaphoreType(size_t initCount)
            : _counter{initCount}
            {
                if (_counter == 0)
                {
                    throw SemaphoreException{"Initial semaphore counter can't be 0!"};
                }
            }

            // disable move and copy
            SemaphoreType(SemaphoreType&&) = delete;

            [[nodiscard]] auto operator co_await() { return awaitable_type{*this}; }

        private:
            void Release()
            {
                std::unique_lock lock{_mtx};

                if (auto topAwaiter = _waiters.pop())
                {
                    lock.unlock();
                    topAwaiter->Notify();
                }
                else
                {
                    ++_counter;
                }
            }

            auto TryAcquire(awaitable_type* awaiter, auto parentCoro)
            {
                std::scoped_lock lock{_mtx};

                if (_counter > 0)
                {
                    --_counter;
                    return true;
                }

                awaiter->PutOnPause(parentCoro);
                _waiters.push(awaiter);
                return false;
            }

            size_t                 _counter;
            StackT<awaitable_type> _waiters;
            std::mutex             _mtx;
        };

        struct SemaphoreAwaiterEvent
        {
            template <typename, typename>
            friend class SemaphoreAwaiter;

            void Notify() const
            {
                if (_notifyCallback)
                {
                    _notifyCallback();
                }
            }

        private:
            void Set(std::invocable auto cb)
            {
                assert(_notifyCallback == nullptr);

                _notifyCallback = cb;
            }

            std::function<void()> _notifyCallback;
        };

        template <typename EventT, typename SemaphoreT>
        class SemaphoreAwaiter
        {
        public:
            SemaphoreAwaiter(SemaphoreT& semaphore)
            : _semaphore{semaphore}
            {
            }

            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                if (_semaphore.TryAcquire(this, parentCoro))
                {
                    return parentCoro;
                }

                return std::noop_coroutine();
            }

            [[nodiscard]] constexpr auto await_resume() noexcept
            {
                return Finally([this] { _semaphore.Release(); });
            }

            void Notify() const { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            SemaphoreAwaiter* next{nullptr};

        private:
            EventT      _event;
            SemaphoreT& _semaphore;
        };

    } // namespace detail

    using Semaphore = detail::SemaphoreType<detail::SemaphoreAwaiter, detail::SemaphoreAwaiterEvent, detail::LinkedPtrStack>;

} // namespace tinycoro

#endif //!__TINY_CORO_SEMAPHORE_H__