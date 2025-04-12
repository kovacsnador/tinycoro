#ifndef TINY_CORO_SYNC_AWAIT_HPP
#define TINY_CORO_SYNC_AWAIT_HPP

#include <cassert>
#include <functional>
#include <utility>
#include <iostream>

#include "PauseHandler.hpp"
#include "Wait.hpp"

namespace tinycoro {

    template <typename SchedulerT, typename EventT, typename FuturesT>
    struct AsyncAwaiterBase
    {
        AsyncAwaiterBase(SchedulerT& scheduler, EventT event, size_t count)
        : _scheduler{scheduler}
        , _event{std::move(event)}
        , _counter{count}
        {
        }

        virtual ~AsyncAwaiterBase() = default;

        // disable copy and move
        AsyncAwaiterBase(AsyncAwaiterBase&&) = delete;

        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] auto await_resume() { return GetAll(this->_futures); }

    protected:
        auto MakeDestroyNotifier()
        {
            return [this] {
                if (this->_counter.fetch_sub(1) == 1)
                {
                    this->_event.Notify();
                }
            };
        }

        SchedulerT&          _scheduler;
        EventT               _event;
        std::atomic<size_t>  _counter;

        FuturesT _futures;
    };

    template <typename... Args>
    struct AsyncAwaiterT;

    template <typename SchedulerT, typename EventT, typename FuturesT, concepts::IsCorouitneTask... Args>
    struct AsyncAwaiterT<SchedulerT, EventT, FuturesT, Args...> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
    {
        AsyncAwaiterT(SchedulerT& scheduler, EventT event, Args&&... args)
        : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, event, sizeof...(Args)}
        , _coroutineTasks(std::forward<Args>(args)...)
        {
        }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            this->_event.Set(context::PauseTask(hdl));

            auto destroyNotifier = this->MakeDestroyNotifier();

            // start all coroutines
            this->_futures = std::apply(
                [destroyNotifier, this]<typename... Ts>(Ts&&... ts) {
                    (ts.SetDestroyNotifier(destroyNotifier), ...);
                    return this->_scheduler.Enqueue(std::forward<Ts>(ts)...);
                },
                std::move(this->_coroutineTasks));
        }

        private:
            std::tuple<Args...>   _coroutineTasks;
    };

    template <typename SchedulerT, typename EventT, typename FuturesT, concepts::Iterable ContainerT>
    struct AsyncAwaiterT<SchedulerT, EventT, FuturesT, ContainerT> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
    {
        AsyncAwaiterT(SchedulerT& scheduler, EventT event, ContainerT&& container)
        : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, event, std::size(container)}
        , _container{std::forward<ContainerT>(container)}
        {
        }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            this->_event.Set(context::PauseTask(hdl));

            auto destroyNotifier = this->MakeDestroyNotifier();

            // setting the destroy notifier callback
            for(auto& it : _container)
            {
                it.SetDestroyNotifier(destroyNotifier);
            }

            // start all coroutines
            this->_futures = this->_scheduler.Enqueue(std::move(_container));
        }

    private:
        ContainerT&& _container;
    };

    template <typename... Args>
    struct AsyncAnyOfAwaiterT;

    template <typename SchedulerT, typename StopSourceT, typename EventT, typename FuturesT, concepts::IsCorouitneTask... Args>
    struct AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, EventT, FuturesT, Args...> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
    {
        AsyncAnyOfAwaiterT(SchedulerT& scheduler, StopSourceT stopSource, EventT event, Args&&... args)
        : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, event, sizeof...(Args)}
        , _stopSource{std::move(stopSource)}
        , _coroutineTasks(std::forward<Args>(args)...)
        {
        }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            this->_event.Set(context::PauseTask(hdl));

            auto destroyNotifier = this->MakeDestroyNotifier();

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
        std::tuple<Args...>   _coroutineTasks;
    };

    template <typename SchedulerT, typename StopSourceT, typename EventT, typename FuturesT, concepts::Iterable ContainerT>
    struct AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, EventT, FuturesT, ContainerT> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
    {
        AsyncAnyOfAwaiterT(SchedulerT& scheduler, StopSourceT stopSource, EventT event, ContainerT&& container)
        : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, event, std::size(container)}
        , _stopSource{std::move(stopSource)}
        , _container{std::forward<ContainerT>(container)}
        {
        }

        void await_suspend(auto hdl)
        {
            // put tast on pause
            this->_event.Set(context::PauseTask(hdl));

            auto destroyNotifier = this->MakeDestroyNotifier();

            // setting the destroy notifier callback
            for(auto& it : _container)
            {
                it.SetDestroyNotifier(destroyNotifier);
                it.SetStopSource(_stopSource);
            }

            // start all coroutines
            this->_futures = this->_scheduler.Enqueue(std::move(_container));
        }

    private:
        StopSourceT  _stopSource;
        ContainerT&& _container;
    };

    template <typename SchedulerT, concepts::Iterable ContainerT>
    [[nodiscard]] auto SyncAwait(SchedulerT& scheduler, ContainerT&& container)
    {
        using FuturesType = decltype(std::declval<SchedulerT>().Enqueue(std::move(container)));
        return AsyncAwaiterT<SchedulerT, detail::PauseCallbackEvent, FuturesType, ContainerT>{scheduler, {}, std::forward<ContainerT>(container)};
    }

    template <typename SchedulerT, concepts::IsCorouitneTask... Args>
    [[nodiscard]] auto SyncAwait(SchedulerT& scheduler, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().Enqueue(std::forward<Args>(args)...));
        return AsyncAwaiterT<SchedulerT, detail::PauseCallbackEvent, FutureTupleType, Args...>{scheduler, {}, std::forward<Args>(args)...};
    }

    template <typename SchedulerT, typename StopSourceT, concepts::Iterable ContainerT>
    [[nodiscard]] auto AnyOfStopSourceAwait(SchedulerT& scheduler, StopSourceT stopSource, ContainerT&& container)
    {
        using FuturesType = decltype(std::declval<SchedulerT>().Enqueue(std::move(container)));
        return AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, detail::PauseCallbackEvent, FuturesType, ContainerT>{
            scheduler, std::move(stopSource), {}, std::forward<ContainerT>(container)};
    }

    template <typename SchedulerT, typename StopSourceT, concepts::IsCorouitneTask... Args>
    [[nodiscard]] auto AnyOfStopSourceAwait(SchedulerT& scheduler, StopSourceT stopSource, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().Enqueue(std::forward<Args>(args)...));
        return AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, detail::PauseCallbackEvent, FutureTupleType, Args...>{
            scheduler, std::move(stopSource), {}, std::forward<Args>(args)...};
    }

    template <typename SchedulerT, typename StopSourceT = std::stop_source, typename... Args>
    [[nodiscard]] auto AnyOfAwait(SchedulerT& scheduler, Args&&... args)
    {
        return AnyOfStopSourceAwait(scheduler, StopSourceT{}, std::forward<Args>(args)...);
    }

} // namespace tinycoro

#endif // TINY_CORO_SYNC_AWAIT_HPP