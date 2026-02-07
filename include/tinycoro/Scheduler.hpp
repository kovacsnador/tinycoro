// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CORO_SCHEDULER_HPP
#define TINY_CORO_CORO_SCHEDULER_HPP

#include <thread>
#include <future>
#include <functional>
#include <list>
#include <mutex>
#include <concepts>
#include <assert.h>
#include <ranges>
#include <cstddef>
#include <memory_resource>

#include "Common.hpp"
#include "LinkedPtrList.hpp"
#include "AtomicQueue.hpp"
#include "SchedulerWorker.hpp"
#include "SchedulableTask.hpp"
#include "Dispatcher.hpp"

namespace tinycoro {

    namespace detail {

        template <typename TaskT, size_t CACHE_SIZE>
        class CoroThreadPool
        {
        public:
            CoroThreadPool(size_t workerThreadCount = std::thread::hardware_concurrency())
            : _stopSource{}
            , _dispatcher{_sharedTasks, _stopSource.get_token()}
            , _stopCallback{_stopSource.get_token(), [this] { _dispatcher.notify_all(); }}
            {
                _AddWorkers(workerThreadCount);
            }

            ~CoroThreadPool()
            {
                // requesting the stop for the worker threads.
                // Note: we are using jthread
                // so we don't need to explicitly join them.
                _stopSource.request_stop();

                // Explicitly join the jthread workers here to ensure proper destruction order.
                // Although jthread automatically joins in its destructor, we must ensure
                // that the jthread is the first member to be destroyed. This is because
                // if the jthread destructor calls join (thread still running) after other members
                // are destroyed, it could lead to dangling references or undefined behavior.
                //
                // By joining here, we guarantee that the jthread has stopped before
                // any other members are destroyed, avoiding potential race conditions
                // or access to invalid memory, if the code is extended in the future.
                for (auto& it : _workerThreads)
                {
                    if (it.joinable())
                    {
                        it.join();
                    }
                }
            }

            // Disable copy and move
            CoroThreadPool(const CoroThreadPool&) = delete;
            CoroThreadPool(CoroThreadPool&&)      = delete;

            auto GetStopToken() const noexcept { return _stopSource.get_token(); }
            auto GetStopSource() const noexcept { return _stopSource; }

            template <template <typename> class FutureStateT = std::promise,
                      typename onTaskFinishWrapperT          = detail::OnTaskFinishCallbackWrapper,
                      concepts::IsSchedulable... CoroTasksT>
                requires concepts::FutureState<FutureStateT<void>> && (sizeof...(CoroTasksT) > 0)
            [[nodiscard]] auto Enqueue(CoroTasksT&&... tasks)
            {
                static_assert((!std::is_reference_v<CoroTasksT> && ...), "Task must be passed as an rvalue (do not use a reference).");

                auto enqueue = [this]<typename T>(T&& task) {
                    // get the result value
                    using desiredValue_t = typename std::remove_cvref_t<T>::value_type;

                    // calculate the future object which will be returned.
                    using futureState_t = detail::FutureTypeGetter<desiredValue_t, FutureStateT>::futureState_t;

                    futureState_t futureState;
                    auto          future = futureState.get_future();

                    this->_EnqueueImpl<onTaskFinishWrapperT>(std::move(futureState), std::move(task));
                    return future;
                };

                if constexpr (sizeof...(CoroTasksT) == 1)
                {
                    return enqueue(std::forward<CoroTasksT>(tasks)...);
                }
                else
                {
                    return std::tuple{enqueue(std::forward<CoroTasksT>(tasks))...};
                }
            }

            template <template <typename> class FutureStateT = std::promise,
                      typename onTaskFinishWrapperT          = detail::OnTaskFinishCallbackWrapper,
                      concepts::Iterable ContainerT>
                requires concepts::FutureState<FutureStateT<void>> && (!std::is_reference_v<ContainerT>)
            [[nodiscard]] auto Enqueue(ContainerT&& tasks)
            {
                static_assert(!std::is_reference_v<ContainerT>, "Task container must be passed as an rvalue (do not use a reference).");

                // get the result value
                using desiredValue_t = typename std::decay_t<ContainerT>::value_type::value_type;

                // calculate the future object which will be returned.
                using futureState_t = detail::FutureTypeGetter<desiredValue_t, FutureStateT>::futureState_t;
                using future_t      = detail::FutureTypeGetter<desiredValue_t, FutureStateT>::future_t;

                std::vector<future_t> futures;
                futures.reserve(std::size(tasks));

                for (auto&& task : tasks)
                {
                    // register tasks and collect all the futures
                    futureState_t futureState{};
                    futures.emplace_back(futureState.get_future());

                    // Enqueue tasks in the scheduler.
                    _EnqueueImpl<onTaskFinishWrapperT>(std::move(futureState), std::move(task));
                }

                return futures;
            }

            template <typename onTaskFinishWrapperT = detail::OnTaskFinishCallbackWrapper,
                      concepts::FutureState   FutureStateT,
                      concepts::IsSchedulable CoroTasksT>
            auto Enqueue(FutureStateT&& futureState, CoroTasksT&& task)
            {
                return _EnqueueImpl<onTaskFinishWrapperT>(std::move(futureState), std::move(task));
            }

        private:
            template <typename OnFinishCbT, typename FutureStateT, concepts::IsSchedulable CoroTaskT>
                requires (!std::is_reference_v<CoroTaskT>)
            bool _EnqueueImpl(FutureStateT&& futureState, CoroTaskT&& coro)
            {
                if (_stopSource.stop_requested() == false && coro.Address())
                {
                    // not allow to enqueue tasks with uninitialized std::coroutine_handler
                    // or if the a stop is requested
                    auto task = MakeSchedulableTask<OnFinishCbT>(std::move(coro), std::move(futureState));

                    while (_stopSource.stop_requested() == false)
                    {
                        auto pushState = _dispatcher.push_state(std::memory_order::relaxed);
                        if (_dispatcher.try_push(std::move(task)))
                        {
                            // the task is pushed
                            // into the queue
                            return true;
                        }
                        else
                        {
                            // wait until we have space in the queue
                            _dispatcher.wait_for_push(pushState);
                        }
                    }
                }
                else
                {
                    // coroutine task is not scheduled.
                    futureState.set_value(std::nullopt);
                }

                return false;
            }

            void _AddWorkers(size_t workerThreadCount)
            {
                assert(workerThreadCount >= 1);

                for ([[maybe_unused]] auto it : std::views::iota(0u, workerThreadCount))
                {
                    _workerThreads.emplace_back(_dispatcher, _stopSource.get_token());
                }
            }

            using queue_t      = detail::AtomicQueue<TaskT, CACHE_SIZE>;
            using dispatcher_t = detail::Dispatcher<queue_t>;

            // currently active/scheduled tasks
            queue_t _sharedTasks;

            // stop_source to support safe cancellation
            std::stop_source _stopSource;

            // std::vector<queue_t> _queues;
            dispatcher_t _dispatcher;

            // the stop callback, which will be triggered
            // if a stop for _stopSource is requested.
            std::stop_callback<std::function<void()>> _stopCallback;

            // Specialize the worker (thread) type
            using Worker_t = SchedulerWorker<decltype(_dispatcher)>;

            // the worker threads which are running the tasks
            std::list<Worker_t> _workerThreads;
        };

        static constexpr size_t DEFAULT_SCHEDULER_CACHE_SIZE = 1024u;

    } // namespace detail

    // Custom scheduler with custom cache size
    template <uint64_t CACHE_SIZE = detail::DEFAULT_SCHEDULER_CACHE_SIZE>
    using CustomScheduler = detail::CoroThreadPool<detail::SchedulableTask, CACHE_SIZE>;

    using Scheduler = detail::CoroThreadPool<detail::SchedulableTask, detail::DEFAULT_SCHEDULER_CACHE_SIZE>;

} // namespace tinycoro

#endif // !TINY_CORO_CORO_SCHEDULER_HPP
