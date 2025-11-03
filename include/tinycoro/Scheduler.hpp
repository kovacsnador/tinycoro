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
#include "PauseHandler.hpp"
#include "LinkedPtrList.hpp"
#include "AtomicQueue.hpp"
#include "SchedulerWorker.hpp"
#include "SchedulableTask.hpp"

namespace tinycoro {

    namespace detail {

        template <typename TaskT, uint64_t CACHE_SIZE>
        class CoroThreadPool
        {
        public:
            CoroThreadPool(size_t workerThreadCount = std::thread::hardware_concurrency())
            : _stopSource{}
            , _stopCallback{_stopSource.get_token(), [this] { helper::RequestStopForQueue(_sharedTasks); }}
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
                if constexpr (sizeof...(CoroTasksT) == 1)
                {
                    return EnqueueImpl<FutureStateT, onTaskFinishWrapperT>(std::forward<CoroTasksT>(tasks)...);
                }
                else
                {
                    return std::tuple{EnqueueImpl<FutureStateT, onTaskFinishWrapperT>(std::forward<CoroTasksT>(tasks))...};
                }
            }

            template <template <typename> class FutureStateT = std::promise,
                      typename onTaskFinishWrapperT          = detail::OnTaskFinishCallbackWrapper,
                      concepts::Iterable ContainerT>
                requires concepts::FutureState<FutureStateT<void>> && (!std::is_reference_v<ContainerT>)
            [[nodiscard]] auto Enqueue(ContainerT&& tasks)
            {
                // get the result value
                using desiredValue_t = typename std::decay_t<ContainerT>::value_type::value_type;

                // calculate the future object which will be returned.
                using future_t = detail::FutureTypeGetter<desiredValue_t, FutureStateT>::future_t;

                std::vector<future_t> futures;
                futures.reserve(std::size(tasks));

                for (auto&& task : tasks)
                {
                    // register tasks and collect all the futures
                    futures.emplace_back(EnqueueImpl<FutureStateT, onTaskFinishWrapperT>(std::move(task)));
                }

                return futures;
            }

        private:
            template <template <typename> class FutureStateT, typename OnFinishCbT, concepts::IsSchedulable CoroTaskT>
                requires (!std::is_reference_v<CoroTaskT>) && concepts::FutureState<FutureStateT<void>>
            [[nodiscard]] auto EnqueueImpl(CoroTaskT&& coro)
            {
                using futureState_t = detail::FutureTypeGetter<typename CoroTaskT::value_type, FutureStateT>::futureState_t;

                futureState_t futureState;
                auto          future = futureState.get_future();

                if (_stopSource.stop_requested() == false && coro.Address())
                {
                    // not allow to enqueue tasks with uninitialized std::coroutine_handler
                    // or if the a stop is requested
                    auto task = MakeSchedulableTask<OnFinishCbT>(std::move(coro), std::move(futureState));

                    // push the task into the queue
                    helper::PushTask(std::move(task), _sharedTasks, _stopSource);
                }
                else
                {
                    // coroutine task is not scheduled.
                    futureState.set_value(std::nullopt);
                }

                return future;
            }

            void _AddWorkers(size_t workerThreadCount)
            {
                assert(workerThreadCount >= 1);

                for ([[maybe_unused]] auto it : std::views::iota(0u, workerThreadCount))
                {
                    _workerThreads.emplace_back(_sharedTasks, _stopSource.get_token());
                }
            }

            // currently active/scheduled tasks
            detail::AtomicQueue<TaskT, CACHE_SIZE> _sharedTasks;

            // stop_source to support safe cancellation
            std::stop_source _stopSource;

            // the stop callback, which will be triggered
            // if a stop for _stopSource is requested.
            std::stop_callback<std::function<void()>> _stopCallback;

            // Specialize the worker (thread) type
            using Worker_t = SchedulerWorker<decltype(_sharedTasks)>;

            // the worker threads which are running the tasks
            std::list<Worker_t> _workerThreads;
        };

        static constexpr uint64_t DEFAULT_SCHEDULER_CACHE_SIZE = 1024u;

    } // namespace detail

    // Custom scheduler with custom cache size
    template <uint64_t CACHE_SIZE = detail::DEFAULT_SCHEDULER_CACHE_SIZE>
    using CustomScheduler = detail::CoroThreadPool<detail::SchedulableTask, CACHE_SIZE>;

    using Scheduler = detail::CoroThreadPool<detail::SchedulableTask, detail::DEFAULT_SCHEDULER_CACHE_SIZE>;

} // namespace tinycoro

#endif // !TINY_CORO_CORO_SCHEDULER_HPP
