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
#include "LinkedPtrQueue.hpp"

using namespace std::chrono_literals;

namespace tinycoro {

    template <std::move_constructible TaskT>
        requires requires (TaskT t) {
            { t->Resume() } -> std::same_as<void>;
            { t->ResumeState() } -> std::same_as<ETaskResumeState>;
            { t->SetPauseHandler([]{}) } -> std::same_as<void>;
        }
    class CoroThreadPool
    {
    public:
        CoroThreadPool(size_t workerThreadCount = std::thread::hardware_concurrency()) { _AddWorkers(workerThreadCount); }

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

        template <template <typename> class FutureStateT = std::promise, concepts::NonIterable... CoroTasksT>
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
            using FutureStateType = FutureStateT<typename std::decay_t<ContainerT>::value_type::promise_type::value_type>;

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
        template <template<typename> class FutureStateT, typename CoroTaksT>
            requires (!std::is_reference_v<CoroTaksT>) && requires (CoroTaksT c) {
                typename CoroTaksT::promise_type::value_type;
                { c.SetPauseHandler(PauseHandlerCallbackT{}) };
            } &&  concepts::FutureState<FutureStateT<void>>
        [[nodiscard]] auto EnqueueImpl(CoroTaksT&& coro)
        {
            FutureStateT<typename CoroTaksT::promise_type::value_type> futureState;

            auto future  = futureState.get_future();
            auto address = coro.Address();

            if (_stopSource.stop_requested() == false && address)
            {
                // not allow to enqueue tasks with uninitialized std::coroutine_handler
                // or if the a stop is requested
                TaskT task = MakeSchedulableTask(std::move(coro), std::move(futureState));
                task->SetPauseHandler(GeneratePauseResume(task.get()));

                {
                    std::scoped_lock lock{_tasksQueueMtx};
                    _tasks.push(task.release());
                }

                _cv.notify_all();
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

                        std::scoped_lock queueLock{_tasksQueueMtx};
                        _tasks.push(taskPtr);

                        // if we notify, we still need to hold the lock.
                        // otherwise other threads can steal the task and finish up before we invoke notify_all.
                        // Note: notify_once would also work but notify_all performs better if there is a lot of tasks at once.
                        _cv.notify_all();
                    }
                }
            };
        }

        inline void _InvokeTask(TaskT& task)
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
                    {
                        std::scoped_lock lock{_tasksQueueMtx};
                        _tasks.push(task.release());
                    }

                    _cv.notify_all();
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
                        // release the unique_ptr
                        auto taskPtr = task.release();

                        // push back into the pause state
                        _pausedTasks.push_front(taskPtr);

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

            for ([[maybe_unused]] auto it : std::views::iota(0u, workerThreadCount))
            {
                _workerThreads.emplace_back(
                    [this](std::stop_token stopToken) {
                        while (stopToken.stop_requested() == false)
                        {
                            // wait on conditional variable
                            std::unique_lock queueLock{_tasksQueueMtx};
                            if (_cv.wait(queueLock, stopToken, [this] { return !_tasks.empty(); }) == false)
                            {
                                // stop was requested
                                return;
                            }

                            {
                                TaskT task{_tasks.pop()};

                                queueLock.unlock();

                                _InvokeTask(task);
                            }

                            // If the worker thread is already running, we will check for tasks in the queue
                            queueLock.lock();
                            while (stopToken.stop_requested() == false && _tasks.empty() == false)
                            {
                                {
                                    TaskT task{_tasks.pop()};

                                    queueLock.unlock();

                                    _InvokeTask(task);
                                }

                                queueLock.lock();
                            }
                        }
                    },
                    _stopSource.get_token());
            }
        }

        void _CleanUpDanglingTasks() noexcept
        {
            auto cleanupLinkedPtrColl = [](auto& coll) {
                auto it = coll.begin();
                while (it != nullptr)
                {
                    auto  next = it->next;
                    TaskT destroyer{it};
                    it = next;
                }
            };

            cleanupLinkedPtrColl(_pausedTasks);
            cleanupLinkedPtrColl(_tasks);
        }

        // currently active/scheduled tasks
        detail::LinkedPtrQueue<typename TaskT::pointer> _tasks;

        // tasks which are in pause state
        detail::LinkedPtrList<typename TaskT::pointer> _pausedTasks;

        // stop_source to support safe cancellation
        std::stop_source _stopSource;

        // mutext to protect the active tasks queue
        std::mutex _tasksQueueMtx;

        // mutext to protect the paused tasks map
        std::mutex _pausedTasksMtx;

        // conditional variable used to notify
        // if we have active tasks in the queue
        std::condition_variable_any _cv;

        // the worker threads which are running the tasks
        std::vector<std::jthread> _workerThreads;
    };

    using Scheduler = CoroThreadPool<SchedulableTask>;

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_SCHEDULER_HPP__
