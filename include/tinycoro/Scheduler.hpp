// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CORO_SCHEDULER_HPP
#define TINY_CORO_CORO_SCHEDULER_HPP

#include <thread>
#include <future>
#include <functional>
#include <concepts>
#include <assert.h>
#include <cstddef>
#include <vector>

#include "Common.hpp"
#include "LinkedPtrList.hpp"
#include "AtomicQueue.hpp"
#include "SchedulerWorker.hpp"
#include "SchedulableTask.hpp"
#include "Dispatcher.hpp"
#include "Exception.hpp"
#include "MPSCPtrQueue.hpp"
#include "WorkGuard.hpp"

namespace tinycoro {

    namespace detail {

        namespace local {

            /// Returns the default worker-thread count for the scheduler.
            ///
            /// `std::thread::hardware_concurrency()` may return `0` on some
            /// platforms when the value is not detectable. This helper clamps
            /// that result to at least `1`, ensuring the scheduler default is
            /// always valid.
            ///
            /// \return Number of worker threads to use by default, never `0`.
            inline auto HardwareConcurrency() noexcept
            {
                return std::max(std::thread::hardware_concurrency(), 1u);
            }

        } // namespace local

        template <typename DispatcherT, template <typename> class WorkerT>
        class WorkerGroup
        {
            // Specialize the worker (thread) type
            using Worker_t = WorkerT<DispatcherT>;

        public:
            // Owns worker instances and their backing jthreads.
            WorkerGroup(size_t workersCount, DispatcherT& dispatcher, std::stop_token token)
            {
                if (workersCount == 0)
                    throw SchedulerException{"workers count cannot be 0"};

                _workers.reserve(workersCount);
                _workerThreads.reserve(workersCount);

                for (size_t i = 0; i < workersCount; ++i)
                {
                    // create workers
                    auto& worker = _workers.emplace_back(std::make_unique<Worker_t>(dispatcher));

                    // start workers in workersThreads
                    _workerThreads.emplace_back([w = worker.get()](auto token) { w->Run(token); }, token);
                }
            }

            // disable copy and move
            WorkerGroup(WorkerGroup&&) = delete;

            // Joins all worker threads (if joinable).
            void Join()
            {
                for (auto& it : _workerThreads)
                {
                    if (it.joinable())
                    {
                        it.join();
                    }
                }
            }

            std::vector<std::unique_ptr<Worker_t>> _workers;
            std::vector<std::jthread>              _workerThreads;
        };

        template <typename EnqueueStrategyT>
        struct TaskEnqueuer
        {
            TaskEnqueuer(EnqueueStrategyT* strategy)
            : _strategy{strategy}
            {
            }

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

                    _strategy->template _EnqueueImpl<onTaskFinishWrapperT>(std::move(futureState), std::move(task));
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
                    _strategy->template _EnqueueImpl<onTaskFinishWrapperT>(std::move(futureState), std::move(task));
                }

                return futures;
            }

            template <typename onTaskFinishWrapperT = detail::OnTaskFinishCallbackWrapper,
                      concepts::FutureState   FutureStateT,
                      concepts::IsSchedulable CoroTasksT>
            auto Enqueue(FutureStateT&& futureState, CoroTasksT&& task)
            {
                return _strategy->template _EnqueueImpl<onTaskFinishWrapperT>(std::move(futureState), std::move(task));
            }

        private:
            EnqueueStrategyT* _strategy{};
        };

        template <typename TaskT, size_t CACHE_SIZE>
        class ParallelScheduler : public TaskEnqueuer<ParallelScheduler<TaskT, CACHE_SIZE>>
        {
            friend tinycoro::WorkGuard tinycoro::MakeWorkGuard<ParallelScheduler>(ParallelScheduler&) noexcept;

            using enqueuer_t = TaskEnqueuer<ParallelScheduler<TaskT, CACHE_SIZE>>;
            friend enqueuer_t;

        public:
            // Construct a scheduler with an internal stop source.
            //
            // The scheduler creates `workerThreadCount` worker threads and binds the
            // stop callback to its own internal stop token.
            //
            // \param workerThreadCount Number of worker threads to start.
            //        Must be greater than zero.
            ParallelScheduler(size_t workerThreadCount = local::HardwareConcurrency())
            : enqueuer_t{this}
            , _dispatcher{_sharedTasks, _stopSource.get_token()}
            , _stopCallback{_stopSource.get_token(), [this] { _dispatcher.notify_all(); }}
            , _workersGroup{workerThreadCount, _dispatcher, _stopSource.get_token()}
            {
            }

            // Construct a scheduler and bridge an external stop token.
            //
            // When `externalToken` is requested, this scheduler requests stop on its
            // internal stop source and wakes dispatcher waiters.
            //
            // \param externalToken External stop token that controls this scheduler.
            // \param workerThreadCount Number of worker threads to start.
            //        Must be greater than zero.
            ParallelScheduler(std::stop_token externalToken, size_t workerThreadCount = local::HardwareConcurrency())
            : enqueuer_t{this}
            , _dispatcher{_sharedTasks, _stopSource.get_token()}
            , _stopCallback{externalToken,
                            [this] {
                                _stopSource.request_stop();
                                _dispatcher.notify_all();
                            }} // listens to the external token
            , _workersGroup{workerThreadCount, _dispatcher, _stopSource.get_token()}
            {
            }

            ~ParallelScheduler()
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
                _workersGroup.Join();
            }

            // Disable copy and move
            ParallelScheduler(ParallelScheduler&&) = delete;

            auto GetStopToken() const noexcept { return _stopSource.get_token(); }
            auto GetStopSource() const noexcept { return _stopSource; }

        private:
            template <typename OnFinishCbT, typename FutureStateT, concepts::IsSchedulable CoroTaskT>
                requires (!std::is_reference_v<CoroTaskT>)
            bool _EnqueueImpl(FutureStateT&& futureState, CoroTaskT&& coro)
            {
                if (coro.Address() == nullptr)
                    throw SchedulerException{"Coroutine Task is not initialized"};

                // not allow to enqueue tasks with uninitialized std::coroutine_handler
                // or if the a stop is requested
                auto task = MakeSchedulableTask<OnFinishCbT>(std::move(coro), std::move(futureState));
                
                _dispatcher.increase_task_counter(1);

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

                _dispatcher.decrease_task_counter(1);
                // coroutine task is not scheduled.
                return false;
            }

            using queue_t      = detail::AtomicQueue<TaskT, CACHE_SIZE>;
            using dispatcher_t = detail::Dispatcher<queue_t>;

            // currently active/scheduled tasks
            queue_t _sharedTasks;

            // stop_source to support safe cancellation
            std::stop_source _stopSource{};

            // Task dispatcher
            dispatcher_t _dispatcher;

            // the stop callback, which will be triggered
            // if a stop for _stopSource is requested.
            std::stop_callback<std::function<void()>> _stopCallback;

            // contains the assigned workers for this scheduler
            WorkerGroup<dispatcher_t, SchedulerWorker> _workersGroup;
        };

        static constexpr size_t DEFAULT_SCHEDULER_CACHE_SIZE = 1 << 14; // Default queue capacity: 16,384 tasks

        template <typename TaskT>
        class ConcurrentScheduler : public TaskEnqueuer<ConcurrentScheduler<TaskT>>
        {
            friend tinycoro::WorkGuard tinycoro::MakeWorkGuard<ConcurrentScheduler>(ConcurrentScheduler&) noexcept;

            using task_enqueuer_t = TaskEnqueuer<ConcurrentScheduler<TaskT>>;
            friend task_enqueuer_t;

        public:
            // Constructs an concurrent scheduler that runs on the current thread.
            //
            // This scheduler does not spawn worker threads. Tasks enqueued via
            // `Enqueue(...)` are executed when `Run()` is called on the current
            // thread.
            ConcurrentScheduler()
            : task_enqueuer_t{this}
            , _dispatcher{_queue, {}}   // stop_token with no stop state
            , _worker{_dispatcher}
            {
            }

            ~ConcurrentScheduler()
            {
                // debug check for guards
                assert(_workGuardCount.load(std::memory_order::relaxed) == 0);
            }

            // Disable copy and move
            ConcurrentScheduler(ConcurrentScheduler&&) = delete;

            // Runs the scheduler loop on the current thread.
            //
            // Drains all currently queued tasks, exits when no
            // outstanding work remains.
            //
            // In case of a WorkGuard it stays in the loop
            // and wait for new tasks.
            void Run()
            {
                for (;;)
                {
                    auto state = _dispatcher.push_state();

                    // run the next batch
                    _worker.DrainQueuedTasks();

                    // if no refcount break
                    if (_workGuardCount.load(std::memory_order::relaxed) == 0)
                        break;

                    _dispatcher.wait_for_push(state);
                }
            }

        private:
            template <typename OnFinishCbT, typename FutureStateT, concepts::IsSchedulable CoroTaskT>
                requires (!std::is_reference_v<CoroTaskT>)
            bool _EnqueueImpl(FutureStateT&& futureState, CoroTaskT&& coro)
            {
                if (coro.Address() == nullptr)
                    throw SchedulerException{"Coroutine Task is not initialized"};

                // not allow to enqueue tasks with uninitialized std::coroutine_handler
                // or if the a stop is requested
                auto task = MakeSchedulableTask<OnFinishCbT>(std::move(coro), std::move(futureState));

                // push should always succeed
                _dispatcher.increase_task_counter(1);

                auto succeed = _dispatcher.try_push(std::move(task));

                // this shoould never at that point fail.
                assert(succeed);
                return succeed;
            }

            void _Acquire() noexcept { _workGuardCount.fetch_add(1, std::memory_order::relaxed); }

            void _Release() noexcept
            {
                auto last = _workGuardCount.fetch_sub(1, std::memory_order::relaxed);
                assert(last > 0);

                // if this was the last, wake up waiting
                // in the Run() function.
                if (last == 1)
                {
                    _dispatcher.notify_all();
                }
            }

            std::atomic<int32_t> _workGuardCount{};

            using queue_t      = detail::MPSCPtrQueue<typename TaskT::element_type>;
            using dispatcher_t = ConcurrentDispatcher<queue_t, TaskT>;

            queue_t                       _queue;
            dispatcher_t                  _dispatcher;
            SchedulerWorker<dispatcher_t> _worker;
        };

    } // namespace detail

    // Scheduler queue capacity tuning guide (power of two):
    // - 16,384 (1 << 14): good general-purpose library default.
    //
    // - 32,768 (1 << 15): better for sustained high fan-in workloads.
    //
    // - 65,536 (1 << 16) or above: for very bursty/high-throughput scenarios where
    //   reducing producer backpressure is more important than memory footprint.
    //
    // Custom scheduler with explicit queue capacity.
    template <uint64_t CACHE_SIZE = detail::DEFAULT_SCHEDULER_CACHE_SIZE>
    using CustomScheduler = detail::ParallelScheduler<detail::SchedulableTask, CACHE_SIZE>;

    using Scheduler       = detail::ParallelScheduler<detail::SchedulableTask, detail::DEFAULT_SCHEDULER_CACHE_SIZE>;
    using InlineScheduler = detail::ConcurrentScheduler<detail::SchedulableTask>;

} // namespace tinycoro

#endif // !TINY_CORO_CORO_SCHEDULER_HPP
