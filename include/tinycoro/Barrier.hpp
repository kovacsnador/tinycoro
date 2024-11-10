#ifndef __TINY_CORO_BARRIER_HPP__
#define __TINY_CORO_BARRIER_HPP__

#include <barrier>

#include <atomic>

#include "LinkedPtrStack.hpp"
#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {
    namespace detail {

        namespace local {

            auto Decrement(auto value, auto reset)
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
            BarrierAwaiter(EventT event, bool ready)
            : _event{std::move(event)}
            , _ready{ready}
            {
            }

            BarrierAwaiter(BarrierAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept { return _ready; }

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

        auto operator co_await() noexcept { return Wait(); };

        auto Wait()
        {
            return _Wait(false);
        }

        bool Arrive()
        {
            std::unique_lock lock{_mtx};

            _current = detail::local::Decrement(_current, _total);

            if (_current == _total)
            {
                auto waiters = _waiters.steal();
                lock.unlock();

                // notify all waiters
                CompleteAndNotifyAll(waiters);

                return true;
            }
            return false;
        }

        auto ArriveAndWait()
        {
            auto ready = Arrive();
            return _Wait(ready);
        }

        void ArriveAndDrop()
        {
            std::unique_lock lock{_mtx};

            // drop the total count
            if (_total > 0)
            {
                --_total;
            }

            // decrement to _current count
            auto before = _current;
            _current    = detail::local::Decrement(_current, _total);

            if (before == 1)
            {
                auto waiters = _waiters.steal();
                lock.unlock();

                // notify all waiters
                CompleteAndNotifyAll(waiters);
            }
        }

    private:
        awaiter_type _Wait(bool ready)
        {
            auto waiter = awaiter_type{PauseCallbackEvent{}, ready};

            if (std::scoped_lock lock{_mtx}; ready == false)
            {
                _waiters.push(std::addressof(waiter));
            }

            return waiter;
        }

        void CompleteAndNotifyAll(awaiter_type* top)
        {
            if constexpr (std::invocable<CompletionCallbackT>)
            {
                // invoke the complition callback
                std::invoke(_complitionCallback);
            }

            while (top)
            {
                auto next = top->next;
                top->Notify();
                top = next;
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