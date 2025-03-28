#ifndef __TINY_CORO_SCHEDULER_WORKER_HPP__
#define __TINY_CORO_SCHEDULER_WORKER_HPP__

#include <thread>
#include <mutex>
#include <cstddef>

#include "LinkedPtrQueue.hpp"
#include "Common.hpp"

namespace tinycoro { namespace detail {

    namespace helper {
        // With this variable we indicate that
        // a stop purposed by the scheduler
        static constexpr std::nullptr_t SCHEDULER_STOP_EVENT{nullptr};

        bool PushTask(auto task, auto& queue, const auto& stopToken) noexcept
        {
            while (stopToken.stop_requested() == false)
            {
                if (queue.try_push(std::move(task)))
                {
                    // the task is pushed
                    // into the tasks queue
                    return true;
                }
                else
                {
                    // wait until we have space in the queue
                    queue.wait_for_push();
                }
            }

            return false;
        }

        void RequestStopForQueue(auto& queue) noexcept
        {
            // this is necessary to trigger/wake up
            // wait_for_push() waiters
            //
            // this should happen before we push
            // the SCHEDULER_STOP_EVENT into the queue,
            // because this could also remove the
            // SCHEDULER_STOP_EVENT from the queue if we invoke
            // after SCHEDULER_STOP_EVENT push...
            queue.clear();

            // try to push the close event into the task queue
            while (queue.try_push(SCHEDULER_STOP_EVENT) == false)
            {
                // clear the queue and try push
                // SCHEDULER_STOP_EVENT again
                queue.clear();
            }
        }

    } // namespace helper

    template <typename QueueT, typename TaskT>
    class SchedulerWorker
    {
        using TaskElement_t = typename TaskT::element_type;

    public:
        SchedulerWorker(QueueT& taskQueue, std::stop_token stopToken)
        : _sharedTasks{taskQueue}
        , _stopToken{stopToken}
        , _thread{[this](std::stop_token st) { Run(st); }, stopToken}
        {
        }

        ~SchedulerWorker()
        {
            if(joinable())
                join();
        }

        void join()
        {
            // join the thread
            _thread.join();

            // cleans up every task
            _CleanUpDanglingTasks();
        }

        auto joinable() { return _thread.joinable(); }

    private:
        void Run(std::stop_token stopToken)
        {
            while (stopToken.stop_requested() == false)
            {
                _TryToUploadCachedTasks();

                TaskT task{nullptr};
                if (_sharedTasks.try_pop(task))
                {
                    if (task == helper::SCHEDULER_STOP_EVENT)
                    {
                        // if we popped the stop event
                        // out from the queue,
                        // we need to put back for other workers
                        helper::RequestStopForQueue(_sharedTasks);
                    }
                    else
                    {
                        // Invoke the task.
                        // wrapping the task into a TaskT
                        // to make sure, there is a  proper destruction
                        _InvokeTask(std::move(task));
                    }
                }
                else if (stopToken.stop_requested() == false)
                {
                    // wait for new tasks
                    _sharedTasks.wait_for_pop();
                }
            }
        }

        // Generates the pause resume callback
        // It relays on a task pointer address
        PauseHandlerCallbackT GeneratePauseResume(auto taskPtr)
        {
            return [this, taskPtr]() {
                if (_stopToken.stop_requested() == false)
                {
                    auto expected = taskPtr->pauseState.load(std::memory_order_acquire);
                    while (expected != EPauseState::PAUSED)
                    {
                        // If the notify callback invoked very quickly
                        // we have here a little time window to tell to
                        // the scheduler, that the task is ready for resumption
                        if (taskPtr->pauseState.compare_exchange_weak(
                                expected, EPauseState::NOTIFIED, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // The task is notified
                            // in time, so we are done.
                            return;
                        }
                    }

                    // If we reach this point
                    // that means that the task is already in paused state
                    // So we need to resume it manually
                    {
                        std::scoped_lock pauseLock{_pausedTasksMtx};
                        // remove the tasks from paused tasks
                        _pausedTasks.erase(taskPtr);
                    }

                    // push back to the queue
                    // for resumption
                    helper::PushTask(TaskT{taskPtr}, _sharedTasks, _stopToken);
                }
            };
        }

        void _InvokeTask(TaskT&& task)
        {
            // sets the corrent pause resume callback
            // before any resumption
            task->SetPauseHandler(GeneratePauseResume(task.get()));

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

                    // push back the task into the queue
                    //
                    // here potentially we could also just
                    // continue the execution of the task...
                    if (_stopToken.stop_requested() == false)
                    {
                        // no stop was requested
                        // auto taskPtr = task.release();
                        if (_sharedTasks.try_push(std::move(task)))
                        {
                            // try to push back the task
                            // if the queue is not full
                            return;
                        }

                        // the queue is full
                        // so we are saving this task,
                        // and trying to push back into the
                        // shared tasks queue later
                        //
                        // alternatively we could just continue here
                        // with the current taskPtr execution
                        _cachedTasks.push(task.release());
                    }
                    return;
                }
                case PAUSED: {

                    auto expected = task->pauseState.load(std::memory_order_acquire);
                    if (expected != EPauseState::NOTIFIED)
                    {
                        auto taskPtr = task.release();

                        std::scoped_lock pauseLock{_pausedTasksMtx};
                        // push back into the pause state
                        _pausedTasks.push_front(taskPtr);

                        if (taskPtr->pauseState.compare_exchange_strong(
                                expected, EPauseState::PAUSED, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // the task is in the paused task list
                            // we can return
                            return;
                        }

                        // in the meantime the task is notified for resumption
                        // so we need to remove it from the paused task queue
                        // and resume the task
                        _pausedTasks.erase(taskPtr);

                        // reassign the pointer
                        // and continue with this task
                        task.reset(taskPtr);
                    }

                    // we can continue to resume this task
                    continue;
                }
                case STOPPED:
                    [[fallthrough]];
                case DONE:
                    [[fallthrough]];
                default:
                    return;
                }
            }
        }

        void _TryToUploadCachedTasks()
        {
            while (_cachedTasks.empty() == false)
            {
                // get the first task from the cache
                auto  taskPtr = _cachedTasks.begin();
                TaskT task{taskPtr};
                if (_sharedTasks.try_push(std::move(task)))
                {
                    // pop out the task if
                    // we could push into the
                    // shared queue
                    std::ignore = _cachedTasks.pop();
                }
                else
                {
                    // if the push failed
                    // we stop and exit
                    //
                    // we need to release here the taskPtr
                    // because it stays in the cached queue
                    // and otherwise it got just destroyed
                    // in the "TaskT task" destructor.
                    task.release();
                    break;
                }
            }
        }

        void _CleanUpDanglingTasks() noexcept
        {
            auto cleanup = [](auto tasks) {
                auto it = tasks.begin();
                while (it != nullptr)
                {
                    auto  next = it->next;
                    TaskT destroyer{it};
                    it = next;
                }
            };

            // clean up dangling paused tasks
            // after stop was requested
            cleanup(_pausedTasks);

            // clean up dangling cahced tasks
            // after stop was requested
            cleanup(_cachedTasks);
        }

        // the queue which contains all the active tasks
        QueueT& _sharedTasks;

        // mutext to protect the paused tasks map
        std::mutex _pausedTasksMtx;

        // tasks which are in pause state
        detail::LinkedPtrList<typename TaskT::element_type> _pausedTasks;

        // Cache for tasks which could not be push back
        // immediately into the shared task queue.
        //
        // It's intented to mimic the basic task rotation
        // even with a full shared tasks queue.
        detail::LinkedPtrQueue<typename TaskT::element_type> _cachedTasks;

        // The scheduler stop token
        std::stop_token _stopToken;

        // The underlying worker thread
        std::jthread _thread;
    };

}} // namespace tinycoro::detail

#endif //!__TINY_CORO_SCHEDULER_WORKER_HPP__