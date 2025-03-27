#ifndef __TINY_CORO_CORO_SCHEDULER_HPP__
#define __TINY_CORO_CORO_SCHEDULER_HPP__

#include <thread>
#include <future>
#include <functional>
#include <vector>
#include <mutex>
#include <concepts>
#include <assert.h>
#include <ranges>
#include <cstddef>

#include "Future.hpp"
#include "Common.hpp"
#include "PauseHandler.hpp"
#include "PackagedTask.hpp"
#include "LinkedPtrList.hpp"
#include "AtomicQueue.hpp"
#include "SchedulerWorker.hpp"

namespace tinycoro {

    namespace detail {

        template <concepts::IsSchedulable TaskT, std::unsigned_integral auto CACHE_SIZE>
        class CoroThreadPool
        {
            using TaskElement_t = typename TaskT::element_type;

        public:
            CoroThreadPool(size_t workerThreadCount = std::thread::hardware_concurrency())
            : _stopSource{}
            , _stopCallback{_stopSource.get_token(), [this] { _RequestStopForQueue(); }}
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
                // or access to invalid memory.
                for (auto& it : _workerThreads)
                {
                    if (it->joinable())
                    {
                        it->join();
                    }
                }
            }

            // Disable copy and move
            CoroThreadPool(const CoroThreadPool&) = delete;
            CoroThreadPool(CoroThreadPool&&)      = delete;

            auto GetStopToken() const noexcept { return _stopSource.get_token(); }
            auto GetStopSource() const noexcept { return _stopSource; }

            template <template <typename> class FutureStateT = std::promise, concepts::IsCorouitneTask... CoroTasksT>
                requires concepts::FutureState<FutureStateT<void>> && (sizeof...(CoroTasksT) > 0)
            [[nodiscard]] auto Enqueue(CoroTasksT&&... tasks)
            {
                if constexpr (sizeof...(CoroTasksT) == 1)
                {
                    return EnqueueImpl<FutureStateT>(std::forward<CoroTasksT>(tasks)...);
                }
                else
                {
                    return std::tuple{EnqueueImpl<FutureStateT>(std::forward<CoroTasksT>(tasks))...};
                }
            }

            template <template <typename> class FutureStateT = std::promise, concepts::Iterable ContainerT>
                requires concepts::FutureState<FutureStateT<void>>
            [[nodiscard]] auto Enqueue(ContainerT&& tasks)
            {
                // get the result value
                using desiredValue_t = typename std::decay_t<ContainerT>::value_type::value_type;

                // check against void
                // if not void we create a std::optional
                // to support cancellation
                using futureValue_t = detail::FutureReturnT<desiredValue_t>::value_type;

                using FutureStateType = FutureStateT<futureValue_t>;

                std::vector<decltype(std::declval<FutureStateType>().get_future())> futures;
                futures.reserve(std::size(tasks));

                for (auto&& task : tasks)
                {
                    if constexpr (std::is_rvalue_reference_v<decltype(tasks)>)
                    {
                        futures.emplace_back(EnqueueImpl<FutureStateT>(std::move(task)));
                    }
                    else
                    {
                        futures.emplace_back(EnqueueImpl<FutureStateT>(task.TaskView()));
                    }
                }

                return futures;
            }

        private:
            template <template<typename> class FutureStateT, concepts::IsCorouitneTask CoroTaksT>
            requires (!std::is_reference_v<CoroTaksT>) && requires (CoroTaksT c) {
                typename CoroTaksT::value_type;
                { c.SetPauseHandler(PauseHandlerCallbackT{}) };
            } &&  concepts::FutureState<FutureStateT<void>>
        [[nodiscard]] auto EnqueueImpl(CoroTaksT&& coro)
            {
                // get the result value
                using desiredValue_t = typename CoroTaksT::value_type;

                // check against void
                // if not void we create a std::optional
                // to support cancellation
                using futureValue_t = detail::FutureReturnT<desiredValue_t>::value_type;

                FutureStateT<futureValue_t> futureState;

                auto future  = futureState.get_future();
                auto address = coro.Address();

                if (_stopSource.stop_requested() == false && address)
                {
                    // not allow to enqueue tasks with uninitialized std::coroutine_handler
                    // or if the a stop is requested
                    TaskT task = MakeSchedulableTask(std::move(coro), std::move(futureState));

                    // push the task into the queue
                    _PushTask(std::move(task), _stopSource);
                }

                return future;
            }

            void _AddWorkers(size_t workerThreadCount)
            {
                assert(workerThreadCount >= 1);

                _workerThreads.reserve(workerThreadCount);

                for ([[maybe_unused]] auto it : std::views::iota(0u, workerThreadCount))
                {
                    _workerThreads.emplace_back(std::make_unique<Worker_t>(_sharedTasks, _stopSource.get_token()));
                }
            }

            bool _PushTask(TaskT task, const std::stop_source& stopSource) noexcept
            {
                while (stopSource.stop_requested() == false)
                {
                    if (_sharedTasks.try_push(std::move(task)))
                    {
                        // the task is pushed
                        // into the tasks queue
                        return true;
                    }
                    else
                    {
                        // wait until we have space in the queue
                        _sharedTasks.wait_for_push();
                    }
                }

                // make sure that the task
                // is properly destroyed
                // after a failing push call
                //TaskT destroyer{taskPtr};

                return false;
            }

            void _RequestStopForQueue() noexcept
            {
                // this is necessary to trigger/wake up
                // wait_for_push() waiters
                //
                // this should happen before we push
                // the STOP_EVENT into the queue,
                // because this could also remove the
                // STOP_EVENT from the queue...
                _sharedTasks.clear();

                // try to push the close event into the task queue
                while (_sharedTasks.try_push(STOP_EVENT) == false)
                {
                    // clear the queue and try
                    // again with STOP_EVENT
                    _sharedTasks.clear();

                    // try to pop an element
                    // to make place for the stopEvent
                    // and destroy the task explicitly
                    //TaskT task;
                    //std::ignore = _sharedTasks.try_pop(task);

                    /*if (_sharedTasks.try_pop(task))
                    {
                        // destroy the task explicitly
                        TaskT taskDestroyer{taskPtr};
                    }*/
                }
            }

            /*void _TaskCleanUp() noexcept
            {
                // clean up active tasks, which are stuck in the queue
                TaskElement_t* taskPtr{nullptr};
                while (_sharedTasks.empty() == false)
                {
                    TaskT destroyer{taskPtr};
                }
            }*/

            // With this variable we indicate that
            // a stop purposed by the scheduler
            static constexpr std::nullptr_t STOP_EVENT{nullptr};

            // currently active/scheduled tasks
            detail::AtomicQueue<TaskT, CACHE_SIZE> _sharedTasks;

            // stop_source to support safe cancellation
            std::stop_source _stopSource;

            std::stop_callback<std::function<void()>> _stopCallback;

            using Worker_t = SchedulerWorker<decltype(_sharedTasks), TaskT>;

            // the worker threads which are running the tasks
            std::vector<std::unique_ptr<Worker_t>> _workerThreads;
        };

        static constexpr uint32_t DEFAULT_SCHEDULER_CACHE_SIZE = 1024u;

    } // namespace detail

    // Custom scheduler with custom cache size
    template <std::unsigned_integral auto CACHE_SIZE>
    using CustomScheduler = detail::CoroThreadPool<SchedulableTask, CACHE_SIZE>;

    using Scheduler = detail::CoroThreadPool<SchedulableTask, detail::DEFAULT_SCHEDULER_CACHE_SIZE>;

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_SCHEDULER_HPP__
