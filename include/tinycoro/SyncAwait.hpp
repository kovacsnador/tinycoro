#ifndef __TINY_CORO_SYNC_AWAIT_HPP__
#define __TINY_CORO_SYNC_AWAIT_HPP__

#include <cassert>
#include <functional>
#include <utility>
#include <iostream>

#include "PauseHandler.hpp"
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
    struct AsyncAwaiterBase
    {
        AsyncAwaiterBase(SchedulerT& scheduler, EventT event, Args&&... args)
        : _scheduler{scheduler}
        , _event{std::move(event)}
        , _coroutineTasks{std::forward<Args>(args)...}
        {
        }

        virtual ~AsyncAwaiterBase() = default;

        // disable copy and move
        AsyncAwaiterBase(AsyncAwaiterBase&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] auto await_resume() { return GetAll(this->_futures); }

    protected:
        SchedulerT&           _scheduler;
        EventT                _event;
        std::tuple<Args...>   _coroutineTasks;
        std::atomic<uint32_t> _counter{sizeof...(Args)};

        FuturesT _futures;
    };

    template <typename SchedulerT, typename EventT, typename FuturesT, typename... Args>
    struct AsyncAwaiterT : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT, Args...>
    {
        AsyncAwaiterT(SchedulerT& scheduler, EventT event, Args&&... args)
        : AsyncAwaiterBase<SchedulerT, EventT, FuturesT, Args...>{scheduler, event, std::forward<Args>(args)...}
        {
        }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            this->_event.Set(PauseHandler::PauseTask(hdl));

            auto destroyNotifier = [this] {
                if (this->_counter.fetch_sub(1) == 1)
                {
                    this->_event.Notify();
                }
            };

            // start all coroutines
            this->_futures = std::apply(
                [destroyNotifier, this]<typename... Ts>(Ts&&... ts) {
                    (ts.SetDestroyNotifier(destroyNotifier), ...);
                    return this->_scheduler.Enqueue(std::forward<Ts>(ts)...);
                },
                std::move(this->_coroutineTasks));
        }
    };

    template <typename SchedulerT, typename StopSourceT, typename EventT, typename FuturesT, typename... Args>
    struct AsyncAnyOfAwaiterT : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT, Args...>
    {
        AsyncAnyOfAwaiterT(SchedulerT& scheduler, StopSourceT stopSource, EventT event, Args&&... args)
        : AsyncAwaiterBase<SchedulerT, EventT, FuturesT, Args...>{scheduler, event, std::forward<Args>(args)...}
        , _stopSource{std::move(stopSource)}
        {
        }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            this->_event.Set(PauseHandler::PauseTask(hdl));

            auto destroyNotifier = [this] {
                if (this->_counter.fetch_sub(1) == 1)
                {
                    this->_event.Notify();
                }
            };

            // start all coroutines
            this->_futures = std::apply(
                [destroyNotifier, this]<typename... Ts>(Ts&&... ts) {
                    ((ts.SetDestroyNotifier(destroyNotifier), ts.SetStopSource(_stopSource)), ...);
                    return this->_scheduler.Enqueue(std::forward<Ts>(ts)...);
                },
                std::move(this->_coroutineTasks));
        }

    private:
        StopSourceT _stopSource;
    };

    template <typename SchedulerT, typename... Args>
    [[nodiscard]] auto SyncAwait(SchedulerT& scheduler, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().Enqueue(std::forward<Args>(args)...));
        return AsyncAwaiterT<SchedulerT, detail::AsyncAwaiterEvent, FutureTupleType, Args...>{scheduler, {}, std::forward<Args>(args)...};
    }

    template <typename SchedulerT, typename StopSourceT, typename... Args>
    [[nodiscard]] auto AnyOfStopSourceAwait(SchedulerT& scheduler, StopSourceT stopSource, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().Enqueue(std::forward<Args>(args)...));
        return AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, detail::AsyncAwaiterEvent, FutureTupleType, Args...>{
            scheduler, std::move(stopSource), {}, std::forward<Args>(args)...};
    }

    template <typename SchedulerT, typename StopSourceT = std::stop_source, typename... Args>
    [[nodiscard]] auto AnyOfAwait(SchedulerT& scheduler, Args&&... args)
    {
        return AnyOfStopSourceAwait(scheduler, StopSourceT{}, std::forward<Args>(args)...);
    }

} // namespace tinycoro

#endif //!__TINY_CORO_SYNC_AWAIT_HPP__