// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CO_WAIT_HPP
#define TINY_CORO_CO_WAIT_HPP

#include <cassert>
#include <functional>
#include <utility>
#include <iostream>

#include "PauseHandler.hpp"
#include "Wait.hpp"
#include "UnsafeFunction.hpp"

namespace tinycoro {

    namespace detail {

        // Custom onFinish task callback.
        //
        // Not only setting the future object, but also responisble
        // for the awaiter resumption.
        //
        // In our use case with `co_await`, this is essential because
        // we must notify the awaitable object once the coroutine
        // has completed execution and all associated `co_await` tasks are done.
        template <typename AwaitableT>
        struct AsyncAwaitOnFinishWrapper
        {
            template <typename PromiseT, typename FutureStateT>
            [[nodiscard]] static constexpr auto Get() noexcept
            {
                return [](void* promise, void* futureState, std::exception_ptr ex) {
                    // Call the default task finish handler to set the future.
                    detail::OnTaskFinish<PromiseT, FutureStateT>(promise, futureState, std::move(ex));

                    // Notify the current awaitable if all the coroutines are completed.
                    auto  p = static_cast<PromiseT*>(promise);
                    auto* a = static_cast<AwaitableT>(p->CurrentAwaitable());
                    a->DestroyNotify();
                };
            }
        };

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
            constexpr void DestroyNotify() noexcept
            {
                if (_counter.fetch_sub(1, std::memory_order_relaxed) == 1)
                {
                    _event.Notify();
                }
            }

            SchedulerT&         _scheduler;
            EventT              _event;
            std::atomic<size_t> _counter;

            FuturesT _futures;
        };

        template <typename... Args>
        struct AsyncAwaiterT;

        template <typename SchedulerT, typename EventT, typename FuturesT, concepts::IsCorouitneTask... Args>
        struct AsyncAwaiterT<SchedulerT, EventT, FuturesT, Args...> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
        {
            friend struct AsyncAwaitOnFinishWrapper<AsyncAwaiterT*>;

            AsyncAwaiterT(SchedulerT& scheduler, EventT event, Args&&... args)
            : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, std::move(event), sizeof...(Args)}
            , _coroutineTasks(std::forward<Args>(args)...)
            {
            }

            void await_suspend(auto hdl)
            {
                // put tast on pause
                this->_event.Set(context::PauseTask(hdl));

                // start all coroutines
                this->_futures = std::apply(
                    [this]<typename... Ts>(Ts&&... ts) {
                        (ts.SetCurrentAwaitable(this), ...);
                        return this->_scheduler.template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise, AsyncAwaitOnFinishWrapper<decltype(this)>>(
                            std::forward<Ts>(ts)...);
                    },
                    std::move(this->_coroutineTasks));
            }

        private:
            std::tuple<Args...> _coroutineTasks;
        };

        template <typename SchedulerT, typename EventT, typename FuturesT, concepts::Iterable ContainerT>
        struct AsyncAwaiterT<SchedulerT, EventT, FuturesT, ContainerT> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
        {
            friend struct AsyncAwaitOnFinishWrapper<AsyncAwaiterT*>;

            AsyncAwaiterT(SchedulerT& scheduler, EventT event, ContainerT&& container)
            : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, std::move(event), std::size(container)}
            , _container{std::forward<ContainerT>(container)}
            {
            }

            void await_suspend(auto hdl)
            {
                // put tast on pause
                this->_event.Set(context::PauseTask(hdl));

                // setting the destroy notifier callback
                for (auto& it : _container)
                {
                    it.SetCurrentAwaitable(this);
                }

                // start all coroutines
                this->_futures
                    = this->_scheduler
                          .template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise, AsyncAwaitOnFinishWrapper<decltype(this)>>(
                              std::move(_container));
            }

        private:
            ContainerT&& _container;
        };

        template <typename... Args>
        struct AsyncAnyOfAwaiterT;

        template <typename SchedulerT, typename StopSourceT, typename EventT, typename FuturesT, concepts::IsCorouitneTask... Args>
        struct AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, EventT, FuturesT, Args...> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
        {
            friend struct AsyncAwaitOnFinishWrapper<AsyncAnyOfAwaiterT*>;

            AsyncAnyOfAwaiterT(SchedulerT& scheduler, StopSourceT stopSource, EventT event, Args&&... args)
            : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, std::move(event), sizeof...(Args)}
            , _stopSource{std::move(stopSource)}
            , _coroutineTasks(std::forward<Args>(args)...)
            {
            }

            void await_suspend(auto hdl)
            {
                // put tast on pause
                this->_event.Set(context::PauseTask(hdl));

                // start all coroutines
                this->_futures = std::apply(
                    [this]<typename... Ts>(Ts&&... ts) {
                        ((ts.SetCurrentAwaitable(this), ts.SetStopSource(_stopSource)), ...);
                        return this->_scheduler
                            .template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise, AsyncAwaitOnFinishWrapper<decltype(this)>>(
                            std::forward<Ts>(ts)...);
                    },
                    std::move(this->_coroutineTasks));
            }

        private:
            StopSourceT         _stopSource;
            std::tuple<Args...> _coroutineTasks;
        };

        template <typename SchedulerT, typename StopSourceT, typename EventT, typename FuturesT, concepts::Iterable ContainerT>
        struct AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, EventT, FuturesT, ContainerT> : public AsyncAwaiterBase<SchedulerT, EventT, FuturesT>
        {
            friend struct AsyncAwaitOnFinishWrapper<AsyncAnyOfAwaiterT*>;

            AsyncAnyOfAwaiterT(SchedulerT& scheduler, StopSourceT stopSource, EventT event, ContainerT&& container)
            : AsyncAwaiterBase<SchedulerT, EventT, FuturesT>{scheduler, std::move(event), std::size(container)}
            , _stopSource{std::move(stopSource)}
            , _container{std::forward<ContainerT>(container)}
            {
            }

            void await_suspend(auto hdl)
            {
                // put tast on pause
                this->_event.Set(context::PauseTask(hdl));

                // setting the destroy notifier callback
                for (auto& it : _container)
                {
                    it.SetCurrentAwaitable(this);
                    it.SetStopSource(_stopSource);
                }

                // start all coroutines
                this->_futures
                    = this->_scheduler
                          .template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise, AsyncAwaitOnFinishWrapper<decltype(this)>>(
                              std::move(_container));
            }

        private:
            StopSourceT  _stopSource;
            ContainerT&& _container;
        };

    } // namespace detail

    template <typename SchedulerT, concepts::Iterable ContainerT>
    [[nodiscard]] auto AllOfAwait(SchedulerT& scheduler, ContainerT&& container)
    {
        using FuturesType = decltype(std::declval<SchedulerT>().template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise>(std::move(container)));
        return detail::AsyncAwaiterT<SchedulerT, detail::ResumeSignalEvent, FuturesType, ContainerT>{
            scheduler, {}, std::forward<ContainerT>(container)};
    }

    template <typename SchedulerT, concepts::IsCorouitneTask... Args>
        requires (sizeof...(Args) > 0)
    [[nodiscard]] auto AllOfAwait(SchedulerT& scheduler, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise>(std::forward<Args>(args)...));
        return detail::AsyncAwaiterT<SchedulerT, detail::ResumeSignalEvent, FutureTupleType, Args...>{scheduler, {}, std::forward<Args>(args)...};
    }

    template <typename SchedulerT, concepts::IsStopSource StopSourceT, concepts::Iterable ContainerT>
    [[nodiscard]] auto AnyOfAwait(SchedulerT& scheduler, StopSourceT stopSource, ContainerT&& container)
    {
        using FuturesType = decltype(std::declval<SchedulerT>().template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise>(std::move(container)));
        return detail::AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, detail::ResumeSignalEvent, FuturesType, ContainerT>{
            scheduler, std::move(stopSource), {}, std::forward<ContainerT>(container)};
    }

    template <typename SchedulerT, concepts::IsStopSource StopSourceT, concepts::IsCorouitneTask... Args>
        requires (sizeof...(Args) > 0)
    [[nodiscard]] auto AnyOfAwait(SchedulerT& scheduler, StopSourceT stopSource, Args&&... args)
    {
        using FutureTupleType = decltype(std::declval<SchedulerT>().template Enqueue<tinycoro::EOwnPolicy::OWNER, tinycoro::unsafe::Promise>(std::forward<Args>(args)...));
        return detail::AsyncAnyOfAwaiterT<SchedulerT, StopSourceT, detail::ResumeSignalEvent, FutureTupleType, Args...>{
            scheduler, std::move(stopSource), {}, std::forward<Args>(args)...};
    }

    template <concepts::IsStopSource StopSourceT = std::stop_source, typename SchedulerT, concepts::IsCorouitneTask... Args>
        requires (sizeof...(Args) > 0) && concepts::IsScheduler<SchedulerT, Args...>
    [[nodiscard]] auto AnyOfAwait(SchedulerT& scheduler, Args&&... tasks)
    {
        return AnyOfAwait(scheduler, StopSourceT{}, std::forward<Args>(tasks)...);
    }

    template <concepts::IsStopSource StopSourceT = std::stop_source, typename SchedulerT, concepts::Iterable ContainerT>
        requires concepts::IsScheduler<SchedulerT, ContainerT>
    [[nodiscard]] auto AnyOfAwait(SchedulerT& scheduler, ContainerT&& tasks)
    {
        return AnyOfAwait(scheduler, StopSourceT{}, std::forward<ContainerT>(tasks));
    }

} // namespace tinycoro

#endif // TINY_CORO_CO_WAIT_HPP