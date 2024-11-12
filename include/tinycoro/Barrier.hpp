#ifndef __TINY_CORO_BARRIER_HPP__
#define __TINY_CORO_BARRIER_HPP__

#include <barrier>

#include <atomic>

#include "LinkedPtrStack.hpp"
#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts
    {
        template<typename T>
        concept Integral = std::integral<T> || std::unsigned_integral<T>;
    }

    namespace detail {

        namespace local {
            
            template<concepts::Integral T>
            T Decrement(T value, T reset)
            {
                if (value > 0)
                {
                    return --value ? value : reset;
                }
                return reset;
            }

        } // namespace local

        struct NoopComplitionCallback
        {
        };

        template <typename EventT>
        class BarrierAwaiter
        {
        public:
            template <typename BarrierT>
            BarrierAwaiter(BarrierT& barrier, EventT event, bool ready)
            : _event{std::move(event)}
            , _ready{ready}
            {
                if (_ready == false)
                {
                    barrier.Add(this);
                }
            }

            BarrierAwaiter(BarrierAwaiter&&) = delete;

            [[nodiscard]] bool await_ready() noexcept
            {
                std::scoped_lock lock{_mtx};
                return _ready;
            }

            bool await_suspend(auto parentCoro)
            {
                std::scoped_lock lock{_mtx};
                if (_ready == false)
                {
                    // Put on pause
                    _event.Set(PauseHandler::PauseTask(parentCoro));
                    return true;
                }
                return false;
            }

            constexpr void await_resume() const noexcept { }

            void Notify()
            {
                std::scoped_lock lock{_mtx};
                _ready = true;
                _event.Notify();
            }

            BarrierAwaiter* next{nullptr};

        private:
            EventT _event;
            bool   _ready;

            std::mutex _mtx;
        };
    } // namespace detail

    template <typename CompletionCallbackT = detail::NoopComplitionCallback, template <typename> class AwaiterT = detail::BarrierAwaiter>
    class Barrier
    {
        using awaiter_type = AwaiterT<PauseCallbackEvent>;
        friend class AwaiterT<PauseCallbackEvent>;

    public:
        Barrier(size_t initCount, CompletionCallbackT callback = {})
        : _total{initCount}
        , _current{initCount}
        , _complitionCallback{std::move(callback)}
        {
            if (initCount < 1)
            {
                throw BarrierException{"Barrier: Initial count can NOT be 0."};
            }
        }

        // disable move and copy
        Barrier(Barrier&&) = delete;

        [[nodiscard]] auto operator co_await() noexcept { return Wait(); };

        [[nodiscard]] auto Wait()
        {
            std::scoped_lock lock{_mtx};
            return _Wait(false);
        }

        bool Arrive()
        {
            std::unique_lock lock{_mtx};
            return _Arrive(lock);
        }

        [[nodiscard]] auto ArriveAndWait()
        {
            std::unique_lock lock{_mtx};

            auto ready = _Arrive(lock);
            return _Wait(ready);
        }

        bool ArriveAndDrop()
        {
            std::unique_lock lock{_mtx};

            // drop the total count
            if (_total > 1)
            {
                --_total;
            }

            return _Arrive(lock);
        }

    private:
        template <typename MutexT>
        [[nodiscard]] bool _Arrive(std::unique_lock<MutexT>& lock)
        {
            auto before = _current;
            _current = detail::local::Decrement(_current, _total);

            if (before == 1)
            {
                auto waiters = _waiters.steal();

                // call complition callback
                Complete();

                // unlock
                lock.unlock();

                // notify all waiters
                NotifyAll(waiters);

                return true;
            }
            return false;
        }

        [[nodiscard]] auto _Wait(bool ready) { return awaiter_type{*this, PauseCallbackEvent{}, ready}; }

        void Add(awaiter_type* waiter) { _waiters.push(waiter); }

        void NotifyAll(awaiter_type* top)
        {
            while (top)
            {
                auto next = top->next;
                top->Notify();
                top = next;
            }
        }

        void Complete()
        {
            if constexpr (std::invocable<CompletionCallbackT>)
            {
                // invoke the complition callback
                std::invoke(_complitionCallback);
            }
        }

        std::mutex _mtx;

        size_t _total;
        size_t _current;

        CompletionCallbackT _complitionCallback;

        detail::LinkedPtrStack<awaiter_type> _waiters;
    };

} // namespace tinycoro
#endif //!__TINY_CORO_BARRIER_HPP__