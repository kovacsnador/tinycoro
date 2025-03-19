#ifndef __TINY_CORO_CORO_SCHEDULER_HPP__
#define __TINY_CORO_CORO_SCHEDULER_HPP__

#include <thread>
#include <future>
#include <functional>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <concepts>
#include <assert.h>
#include <ranges>

#include "Future.hpp"
#include "Common.hpp"
#include "PauseHandler.hpp"
#include "PackagedTask.hpp"
#include "LinkedPtrList.hpp"
#include "AtomicQueue.hpp"

using namespace std::chrono_literals;

namespace tinycoro {

    namespace detail {

        template <concepts::IsSchedulable TaskT, std::unsigned_integral auto CACHE_SIZE>
        class CoroThreadPool
        {
            using TaskElement_t = typename TaskT::element_type;

        public:
            CoroThreadPool(size_t workerThreadCount = std::thread::hardware_concurrency())
            : _stopSource{}
            , _stopCallback{_stopSource.get_token(), [&] {
                                // try to push the close event into the task queue
                                while (_tasks.try_push(STOP_EVENT) == false)
                                {
                                    // try to pop an element
                                    // to make place for the stopEvent
                                    TaskElement_t* taskPtr{nullptr};
                                    if (_tasks.try_pop(taskPtr))
                                    {
                                        // destroy the task explicitly
                                        TaskT taskDestroyer{taskPtr};
                                    }
                                }
                            }}
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
                    if (it.joinable())
                    {
                        it.join();
                    }
                }

                // Make some trivial cleanup for dangling tasks
                _CleanUpDanglingTasks();
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
                    task->SetPauseHandler(GeneratePauseResume(task.get()));

                    // push the task into the queue
                    _PushTask(task.release(), _stopSource);
                }

                return future;
            }

            // Generates the pause resume callback
            // It relays on a task pointer address
            PauseHandlerCallbackT GeneratePauseResume(auto taskPtr)
            {
                return [this, taskPtr]() {
                    if (_stopSource.stop_requested() == false)
                    {
                        std::unique_lock pauseLock{_pausedTasksMtx};

                        if (taskPtr->next == nullptr && taskPtr->prev == nullptr && _pausedTasks.begin() != taskPtr)
                        {
                            // indicate that we don't need any sleep here
                            // So notify task that it should be wakeup and put into tasks queue.
                            // we make this with setting the next and a prev pointers to self
                            taskPtr->next = taskPtr;
                            taskPtr->prev = taskPtr;

                            pauseLock.unlock();

                            return;
                        }
                        else
                        {
                            // wake up the task
                            _pausedTasks.erase(taskPtr);

                            // pause lock can be released
                            pauseLock.unlock();

                            // push back to the queue
                            // for resumption
                            _PushTask(taskPtr, _stopSource);
                        }
                    }
                };
            }

            inline void _InvokeTask(TaskT&& task)
            {
                // reset the navigation variables
                task->prev = nullptr;
                task->next = nullptr;

                using enum ETaskResumeState;
                for (;;)
                {
                    // resume the task
                    task->Resume();

                    // get the resume state from the coroutine or corouitne child
                    auto resumeState = task->ResumeState();

                    switch (resumeState)
                    {
                    case SUSPENDED: {

                        // push back to the queue
                        //
                        // here potentially we could also just
                        // continue the execution of the task...
                        _PushTask(task.release(), _stopSource);
                        break;
                    }
                    case PAUSED: {
                        std::unique_lock pauseLock{_pausedTasksMtx};
                        if (task->next == task.get() && task->prev == task.get())
                        {
                            // reset the navigation
                            // the task is already waked up
                            task->next = nullptr;
                            task->prev = nullptr;

                            pauseLock.unlock();

                            // we can continue to resume this task
                            continue;
                        }
                        else
                        {
                            // push back into the pause state
                            _pausedTasks.push_front(task.release());

                            pauseLock.unlock();
                        }
                        break;
                    }
                    case STOPPED:
                        [[fallthrough]];
                    case DONE:
                        break;
                    default:
                        break;
                    }

                    // task is handled regarding on his resumeState
                    break;
                }
            }

            void _AddWorkers(size_t workerThreadCount)
            {
                assert(workerThreadCount >= 1);

                _workerThreads.reserve(workerThreadCount);

                for ([[maybe_unused]] auto it : std::views::iota(0u, workerThreadCount))
                {
                    _workerThreads.emplace_back(
                        [this](std::stop_token stopToken) {
                            while (stopToken.stop_requested() == false)
                            {
                                // wait for new tasks
                                _tasks.wait_for_element();

                                TaskElement_t* taskPtr{nullptr};
                                while (_tasks.try_pop(taskPtr) && stopToken.stop_requested() == false)
                                {
                                    if (taskPtr != STOP_EVENT)
                                    {
                                        // wrapping the task into a TaskT
                                        // to make sure, there is a  proper destruction
                                        _InvokeTask(TaskT{taskPtr});
                                    }
                                }
                            }
                        },
                        _stopSource.get_token());
                }
            }

            void _CleanUpDanglingTasks() noexcept
            {
                // clean up dangling paused tasks
                // after stop was requested
                auto it = _pausedTasks.begin();
                while (it != nullptr)
                {
                    auto  next = it->next;
                    TaskT destroyer{it};
                    it = next;
                }

                // clean up active tasks, which are stuck in the queue
                TaskElement_t* taskPtr{nullptr};
                while (_tasks.try_pop(taskPtr))
                {
                    TaskT destroyer{taskPtr};
                }
            }

            bool _PushTask(TaskElement_t* taskPtr, const std::stop_source& stopSource)
            {
                while (stopSource.stop_requested() == false)
                {
                    if (_tasks.try_push(taskPtr))
                    {
                        // the task is pushed
                        // into the tasks queue
                        return true;
                    }
                }

                // make sure that the task
                // is properly destroyed
                // after a failing push call
                TaskT destroyer{taskPtr};

                return false;
            }

            // With this variable we indicate that
            // a stop purposed by the scheduler
            static constexpr TaskElement_t* STOP_EVENT{nullptr};

            // currently active/scheduled tasks
            detail::AtomicQueue<TaskElement_t*, CACHE_SIZE> _tasks;

            // tasks which are in pause state
            detail::LinkedPtrList<TaskElement_t> _pausedTasks;

            // stop_source to support safe cancellation
            std::stop_source _stopSource;

            std::stop_callback<std::function<void()>> _stopCallback;

            // mutext to protect the paused tasks map
            std::mutex _pausedTasksMtx;

            // the worker threads which are running the tasks
            std::vector<std::jthread> _workerThreads;
        };

        static constexpr uint32_t DEFAULT_SCHEDULER_CACHE_SIZE = 1024u;

    } // namespace detail

    // Custom scheduler with custom cache size
    template <std::unsigned_integral auto CACHE_SIZE>
    using CustomScheduler = detail::CoroThreadPool<SchedulableTask, CACHE_SIZE>;

    using Scheduler = detail::CoroThreadPool<SchedulableTask, detail::DEFAULT_SCHEDULER_CACHE_SIZE>;

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_SCHEDULER_HPP__
