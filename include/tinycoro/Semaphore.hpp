#ifndef __TINY_CORO_SEMAPHORE_H__
#define __TINY_CORO_SEMAPHORE_H__

#include <mutex>
#include <coroutine>
#include <stdexcept>

#include "PauseHandler.hpp"
#include "LinkedPtrStack.hpp"
#include "ReleaseGuard.hpp"

namespace tinycoro {

    struct SemaphoreException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    namespace detail {

        template <template <typename, typename> class AwaitableT, template <typename> class StackT>
        class Semaphore
        {
        public:
            using awaitable_type = AwaitableT<Semaphore, detail::PauseCallbackEvent>;

            friend class AwaitableT<Semaphore, detail::PauseCallbackEvent>;
            friend class ReleaseGuard<Semaphore>;

            Semaphore(size_t initCount)
            : _counter{initCount}
            {
                if (_counter == 0)
                {
                    throw SemaphoreException{"Initial semaphore counter can't be 0!"};
                }
            }

            // disable move and copy
            Semaphore(Semaphore&&) = delete;

            [[nodiscard]] auto operator co_await() { return awaitable_type{*this, detail::PauseCallbackEvent{}}; }

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

        template <typename SemaphoreT, typename EventT>
        class SemaphoreAwaiter
        {
        public:
            SemaphoreAwaiter(SemaphoreT& semaphore, EventT event)
            : _semaphore{semaphore}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            SemaphoreAwaiter(SemaphoreAwaiter&&) = delete;

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
                return ReleaseGuard{_semaphore};
            }

            void Notify() const { _event.Notify(); }

            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            SemaphoreAwaiter* next{nullptr};

        private:
            SemaphoreT& _semaphore;
            EventT      _event;
        };

    } // namespace detail

    using Semaphore = detail::Semaphore<detail::SemaphoreAwaiter, detail::LinkedPtrStack>;

} // namespace tinycoro

#endif //!__TINY_CORO_SEMAPHORE_H__