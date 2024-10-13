#ifndef __TINY_CORO_SEMAPHORE_H__
#define __TINY_CORO_SEMAPHORE_H__

#include <mutex>
#include <coroutine>
#include <syncstream>

#include "PauseHandler.hpp"
#include "Finally.hpp"

auto SyncOut2(std::ostream& stream = std::cout)
{
    return std::osyncstream{stream};
}

namespace tinycoro {

    template <typename NodeT>
        requires requires (NodeT n) {
            { n.next } -> std::same_as<NodeT*&>;
        }
    struct PtrStack
    {
        void push(NodeT* newNode)
        {
            assert(newNode);

            newNode->next = _top;
            _top          = newNode;
        }

        NodeT* pop()
        {
            auto top = _top;
            if (top)
            {
                _top = _top->next;
            }
            return top;
        }

        [[nodiscard]] constexpr NodeT* top() noexcept { return _top; }

        [[nodiscard]] constexpr bool empty() const noexcept { return _top; }

    private:
        NodeT* _top{nullptr};
    };

    template <template <typename, typename> class AwaitableT, typename EventT, template <typename> class StackT>
    struct SemaphoreType
    {
        using awaitable_type = AwaitableT<EventT, SemaphoreType>;

        friend struct AwaitableT<EventT, SemaphoreType>;

        SemaphoreType(size_t initCount)
        : _counter{initCount}
        {
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

        size_t                   _counter;
        PtrStack<awaitable_type> _waiters;
        std::mutex               _mtx;
    };

    struct SemaphoreAwaiterEvent
    {
        template <typename, typename>
        friend struct SemaphoreAwaiter;

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
    struct SemaphoreAwaiter
    {
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

        void Notify() const { _event.Notify(); }

        void PutOnPause(auto parentCoro)
        { 
            _event.Set(PauseHandler::PauseTask(parentCoro));
        }

        [[nodiscard]] constexpr auto await_resume() noexcept
        {
            return Finally([this] { _semaphore.Release(); });
        }

        SemaphoreAwaiter* next{nullptr};

    private:
        EventT      _event;
        SemaphoreT& _semaphore;
    };

    using Semaphore = SemaphoreType<SemaphoreAwaiter, SemaphoreAwaiterEvent, PtrStack>;

} // namespace tinycoro

#endif //!__TINY_CORO_SEMAPHORE_H__