#ifndef TINY_CORO_LATCH_HPP
#define TINY_CORO_LATCH_HPP

#include <mutex>

#include "LinkedPtrStack.hpp"
#include "PauseHandler.hpp"
#include "Exception.hpp"
#include "AwaiterHelper.hpp"

namespace tinycoro {
    namespace detail {

        template <template <typename, typename> class AwaiterT>
        class Latch
        {
        public:
            friend class AwaiterT<Latch, detail::PauseCallbackEvent>;

            using awaiter_type = AwaiterT<Latch, detail::PauseCallbackEvent>;

            Latch(size_t count)
            : _count{count}
            {
                if (count == 0)
                {
                    throw LatchException{"Latch: Initial counter cant be 0."};
                }
            }

            // disable move and copy
            Latch(Latch&&) = delete;

            [[nodiscard]] auto operator co_await() noexcept { return Wait(); };

            [[nodiscard]] auto Wait() noexcept { return awaiter_type{*this, detail::PauseCallbackEvent{}}; }

            [[nodiscard]] auto ArriveAndWait() noexcept
            {
                CountDown();
                return Wait();
            }

            void CountDown() noexcept
            {
                std::unique_lock lock{_mtx};

                if (_count > 0)
                {
                    --_count;
                }

                if (_count == 0)
                {
                    auto top = _waiters.steal();
                    lock.unlock();

                    detail::IterInvoke(top, &awaiter_type::Notify);
                }
            }

        private:
            [[nodiscard]] bool IsReady() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return _count == 0;
            }

            [[nodiscard]] bool Add(awaiter_type* waiter) noexcept
            {
                std::scoped_lock lock{_mtx};
                if (_count)
                {
                    _waiters.push(waiter);
                }

                return _count;
            }

            bool Cancel(awaiter_type* waiter)
            {
                std::scoped_lock lock{_mtx};
                return _waiters.erase(waiter);
            }

            size_t             _count;
            mutable std::mutex _mtx;

            detail::LinkedPtrStack<awaiter_type> _waiters;
        };

        template <typename LatchT, typename CallbackEventT>
        class LatchAwaiter : public detail::SingleLinkable<LatchAwaiter<LatchT, CallbackEventT>>
        {
        public:
            LatchAwaiter(LatchT& latch, CallbackEventT event)
            : _latch{latch}
            , _event{std::move(event)}
            {
            }

            // disabe move and copy
            LatchAwaiter(LatchAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() const noexcept { return _latch.IsReady(); }

            constexpr auto await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_latch.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            constexpr void await_resume() const noexcept { }

            void Notify() const noexcept { _event.Notify(); }

            bool Cancel() noexcept { return _latch.Cancel(this); }

            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

        private:
            LatchT&        _latch;
            CallbackEventT _event;
        };
    } // namespace detail

    using Latch = detail::Latch<detail::LatchAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_LATCH_HPP