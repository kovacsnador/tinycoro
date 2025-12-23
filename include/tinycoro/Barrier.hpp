// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_BARRIER_HPP
#define TINY_CORO_BARRIER_HPP

#include "LinkedPtrStack.hpp"
#include "PauseHandler.hpp"
#include "Exception.hpp"
#include "Finally.hpp"
#include "AwaiterHelper.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro {

    namespace concepts {
        template <typename T>
        concept Integral = std::integral<T> || std::unsigned_integral<T>;
    }

    namespace detail {

        namespace local {

            template <concepts::Integral T>
            constexpr T Decrement(T value, T reset)
            {
                if (value > 0)
                {
                    return --value ? value : reset;
                }
                return reset;
            }

            template <typename PossibleCallableT>
            bool SafeRegularInvoke(const PossibleCallableT& maybeCallable)
            {
                if constexpr (std::regular_invocable<PossibleCallableT>)
                {
                    // invoke the complition callback
                    std::invoke(maybeCallable);
                    return true;
                }
                return false;
            }

            template <typename T>
            void NotifyAll(T* awaiter)
            {
                detail::IterInvoke(awaiter, &T::Notify);
            }

        } // namespace local

        struct NoopComplitionCallback
        {
        };

        enum class EBarrierAwaiterState
        {
            WAIT,
            ARRIVE_AND_WAIT,
            ARRIVE_AND_DROP
        };

        template <typename BarrierT, typename EventT>
        class BarrierAwaiter : public detail::SingleLinkable<BarrierAwaiter<BarrierT, EventT>>
        {
        public:
            BarrierAwaiter(BarrierT& barrier, EventT event, EBarrierAwaiterState policy)
            : _barrier{barrier}
            , _event{std::move(event)}
            , _policy{policy}
            {
            }

            BarrierAwaiter(BarrierAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

            [[nodiscard]] bool await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_barrier.Add(this, _policy) == false)
                {
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            constexpr void await_resume() const noexcept { }

            bool Notify() const noexcept { return _event.Notify(ENotifyPolicy::RESUME); }
            
            bool NotifyToDestroy() const noexcept { return _event.Notify(ENotifyPolicy::DESTROY); }

            bool Cancel() noexcept { return _barrier.Cancel(this); };

        private:
            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            BarrierT& _barrier;
            EventT    _event;

            EBarrierAwaiterState _policy;
        };
    } // namespace detail

    template <typename CompletionCallbackT = detail::NoopComplitionCallback, template <typename, typename> class AwaiterT = detail::BarrierAwaiter>
    class Barrier
    {
        using awaiter_type = AwaiterT<Barrier, detail::ResumeSignalEvent>;
        friend class AwaiterT<Barrier, detail::ResumeSignalEvent>;

    public:
        Barrier(size_t initCount, CompletionCallbackT callback = {})
        : _total{initCount}
        , _current{initCount}
        , _completionCallback{std::move(callback)}
        {
            if (initCount < 1)
            {
                throw BarrierException{"Barrier: Initial count can NOT be 0."};
            }
        }

        // disable move and copy
        Barrier(Barrier&&) = delete;

        [[nodiscard]] auto operator co_await() noexcept { return Wait(); };

        [[nodiscard]] auto Wait() { return MakeAwaiter(detail::EBarrierAwaiterState::WAIT); }

        bool Arrive()
        {
            std::unique_lock lock{_mtx};
            return _Arrive(lock);
        }

        [[nodiscard]] auto ArriveAndWait() { return MakeAwaiter(detail::EBarrierAwaiterState::ARRIVE_AND_WAIT); }

        bool ArriveAndDrop()
        {
            std::unique_lock lock{_mtx};

            // drop the total count
            DecrementTotal();

            return _Arrive(lock);
        }

        [[nodiscard]] auto ArriveDropAndWait() { return MakeAwaiter(detail::EBarrierAwaiterState::ARRIVE_AND_DROP); }

    private:
        template <typename MutexT>
        [[nodiscard]] bool _Arrive(std::unique_lock<MutexT>& lock)
        {
            assert(lock.owns_lock());

            auto before = _current;
            _current    = detail::local::Decrement(_current, _total);

            if (before == 1)
            {
                auto waiters = _waiters.steal();

                // to support exceptions in complition handler
                auto finalAction = Finally([waiters] {
                    // notify all waiters
                    detail::local::NotifyAll(waiters);
                });

                // call complition callback
                detail::local::SafeRegularInvoke(_completionCallback);

                // unlock the mutex
                lock.unlock();

                return true;
            }
            return false;
        }

        [[nodiscard]] auto MakeAwaiter(detail::EBarrierAwaiterState policy) { return awaiter_type{*this, detail::ResumeSignalEvent{}, policy}; }

        [[nodiscard]] bool Add(awaiter_type* waiter, detail::EBarrierAwaiterState policy)
        {
            std::unique_lock lock{_mtx};

            using enum detail::EBarrierAwaiterState;

            bool ready = false;

            if (policy == ARRIVE_AND_WAIT)
            {
                ready = _Arrive(lock);
            }
            else if (policy == ARRIVE_AND_DROP)
            {
                // drop the total count
                DecrementTotal();

                ready = _Arrive(lock);
            }

            if (ready == false)
            {
                // _mtx still holds the lock
                assert(lock.owns_lock());

                _waiters.push(waiter);
            }

            return !ready;
        }

        bool Cancel(awaiter_type* awaiter) noexcept
        {
            std::scoped_lock lock{_mtx};
            return _waiters.erase(awaiter);
        }

        void DecrementTotal() noexcept
        {
            if (_total > 1)
            {
                --_total;
            }
        }

        std::mutex _mtx;

        size_t _total;
        size_t _current;

        CompletionCallbackT _completionCallback;

        // we can use here stack, because we steal all the values from there
        detail::LinkedPtrStack<awaiter_type> _waiters;
    };

} // namespace tinycoro
#endif // TINY_CORO_BARRIER_HPP