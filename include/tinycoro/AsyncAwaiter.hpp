#ifndef __TINY_CORO_ASYNC_AWAITER_HPP__
#define __TINY_CORO_ASYNC_AWAITER_HPP__

#include <cassert>
#include <functional>
#include <utility>

#include "PauseHandler.hpp"
#include "Finally.hpp"
#include "Wait.hpp"

namespace tinycoro {

    namespace detail {

        struct AsyncAwaiterEvent
        {
            void Notify() const
            {
                if (_notifyCallback)
                {
                    _notifyCallback();
                }
            }
        
            void Set(std::invocable auto cb)
            {
                assert(_notifyCallback == nullptr);

                _notifyCallback = cb;
            }

        private:
            std::function<void()> _notifyCallback;
        };

    } // namespace detail

    template <typename SchedulerT, typename EventT, typename FuturesT, typename... Args>
    struct AsyncAwaiterT
    {
        AsyncAwaiterT(SchedulerT& scheduler, Args&&... args)
        : _scheduler{scheduler}
        , _coroutineTasks{std::forward<Args>(args)...}
        {
        }

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        void await_suspend(auto hdl) noexcept
        {
            // put tast on pause
            _event.Set(PauseHandler::PauseTask(hdl));

            auto destroyNotifier = [this] {
                if (_counter.fetch_sub(1) == 1)
                {
                    _event.Notify();
                }
            };

            // start all coroutines
            _result = std::apply(
                [destroyNotifier, this]<typename... Ts>(Ts&&... ts) {
                    (ts.SetDestroyNotifier(destroyNotifier), ...);
                    return _scheduler.EnqueueTasks(std::forward<Ts>(ts)...);
                },
                std::move(_coroutineTasks));
        }

        [[nodiscard]] auto await_resume() noexcept { return GetAll(_result); }

    private:
        SchedulerT&           _scheduler;
        EventT                _event;
        std::tuple<Args...>   _coroutineTasks;
        std::atomic<uint32_t> _counter{sizeof...(Args)};

        FuturesT _result;
    };

    template <typename SchedulerT, typename... Args>
    [[nodiscard]] auto SyncAwait(SchedulerT& scheduler, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().EnqueueTasks(std::forward<Args>(args)...));
        return AsyncAwaiterT<SchedulerT, detail::AsyncAwaiterEvent, FutureTupleType, Args...>{scheduler, std::forward<Args>(args)...};
    }

} // namespace tinycoro

#endif //!__TINY_CORO_ASYNC_AWAITER_HPP__