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
#include "PackagedTask.hpp"
#include "LinkedPtrList.hpp"
#include "AtomicQueue.hpp"
#include "SchedulerWorker.hpp"

namespace tinycoro {

    namespace detail {

        template <concepts::IsSchedulable TaskT, uint64_t CACHE_SIZE, concepts::IsAllocator AllocatorT>
        class CoroThreadPool
        {
        public:
            CoroThreadPool(size_t workerThreadCount = std::thread::hardware_concurrency())
            : _allocator{&_defaultPoolResource}
            , _stopSource{}
            , _stopCallback{_stopSource.get_token(), [this] { helper::RequestStopForQueue(_sharedTasks); }}
            {
                _AddWorkers(workerThreadCount);
            }

            // Thread pool with custom allocator
            CoroThreadPool(AllocatorT allocator, size_t workerThreadCount = std::thread::hardware_concurrency())
            : _allocator{std::move(allocator)}
            , _stopSource{}
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

            template <template <typename> class FutureStateT = std::promise, concepts::IsCorouitneTask... CoroTasksT>
                requires concepts::FutureState<FutureStateT<void>> && (sizeof...(CoroTasksT) > 0)
            [[nodiscard]] auto Enqueue(CoroTasksT&&... tasks)
            {
                if constexpr (sizeof...(CoroTasksT) == 1)
                {
                    return EnqueueImpl<FutureStateT>(_allocator, std::forward<CoroTasksT>(tasks)...);
                }
                else
                {
                    return std::tuple{EnqueueImpl<FutureStateT>(_allocator, std::forward<CoroTasksT>(tasks))...};
                }
            }

            template <template <typename> class FutureStateT = std::promise, concepts::Iterable ContainerT>
                requires concepts::FutureState<FutureStateT<void>> && (!std::is_reference_v<ContainerT>)
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
                    // register tasks and collect all the futures
                    futures.emplace_back(EnqueueImpl<FutureStateT>(_allocator, std::move(task)));
                }

                return futures;
            }

        private:
            template <template<typename> class FutureStateT, concepts::IsCorouitneTask CoroTaksT>
            requires (!std::is_reference_v<CoroTaksT>) && requires (CoroTaksT c) {
                typename CoroTaksT::value_type;
                { c.SetPauseHandler(PauseHandlerCallbackT{}) };
            } &&  concepts::FutureState<FutureStateT<void>>
        [[nodiscard]] auto EnqueueImpl(AllocatorT& allocator, CoroTaksT&& coro)
            {
                // get the result value
                using desiredValue_t = typename CoroTaksT::value_type;

                // check against void
                // if not void we create a std::optional
                // to support cancellation
                using futureValue_t = typename detail::FutureReturnT<desiredValue_t>::value_type;

                FutureStateT<futureValue_t> futureState;

                auto future  = futureState.get_future();
                auto address = coro.Address();

                if (_stopSource.stop_requested() == false && address)
                {
                    // not allow to enqueue tasks with uninitialized std::coroutine_handler
                    // or if the a stop is requested
                    TaskT task = MakeSchedulableTask(std::move(coro), std::move(futureState), allocator);

                    // push the task into the queue
                    helper::PushTask(std::move(task), _sharedTasks, _stopSource);
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

            // as a default setup for the scheduler,
            // we use a pmr synchronized_pool_resource
            // with an allocator.
            //
            // this improves overall speed in the scheduler,
            // but can have a bigger footprint in the memory.
            std::pmr::synchronized_pool_resource _defaultPoolResource{};

            // The dedicated allocator for the scheduler.
            //
            // Can be passed as argument through construction,
            // in that case we igone the _defaultPoolResource.
            AllocatorT _allocator;

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

    using DefaultAllocator_t = std::pmr::polymorphic_allocator<std::byte>;

    // Custom scheduler with custom cache size
    template <uint64_t CACHE_SIZE = detail::DEFAULT_SCHEDULER_CACHE_SIZE, typename AllocatorT = DefaultAllocator_t>
    using CustomScheduler = detail::CoroThreadPool<detail::SchedulableTask<AllocatorT>, CACHE_SIZE, AllocatorT>;

    using Scheduler = detail::CoroThreadPool<detail::SchedulableTask<DefaultAllocator_t>, detail::DEFAULT_SCHEDULER_CACHE_SIZE, DefaultAllocator_t>;

} // namespace tinycoro

#endif // !TINY_CORO_CORO_SCHEDULER_HPP
