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

        bool PushTask(auto task, auto& queue, const auto& stopObject) noexcept
        {
            while (stopObject.stop_requested() == false)
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

        template <typename QueueT>
        void RequestStopForQueue(QueueT& queue) noexcept
        {
            // this is necessary to trigger/wake up
            // wait_for_push() waiters
            //
            // this should happen before we push
            // the SCHEDULER_STOP_EVENT into the queue,
            // because this could also remove the
            // SCHEDULER_STOP_EVENT from the queue if we invoke
            // after SCHEDULER_STOP_EVENT push...
            while (queue.full())
            {
                typename QueueT::value_type destroyer{nullptr};
                if (queue.try_pop(destroyer))
                {
                    // erase at least 1 element
                    break;
                }
            }

            // try to push the close event into the task queue
            while (queue.try_push(SCHEDULER_STOP_EVENT) == false)
            {
                // try to remove one element
                // in order to make place for
                // SCHEDULER_STOP_EVENT
                typename QueueT::value_type destroyer{nullptr};
                std::ignore = queue.try_pop(destroyer);
            }
        }

    } // namespace helper

    template <typename QueueT>
    class SchedulerWorker
    {
        using Task_t = typename QueueT::value_type;

    public:
        SchedulerWorker(QueueT& taskQueue, std::stop_token stopToken)
        : _sharedTasks{taskQueue}
        , _stopToken{stopToken}
        , _thread{[this](std::stop_token st) { Run(st); }, stopToken}
        {
        }

        // disable copy and move
        SchedulerWorker(SchedulerWorker&&) = delete;

        ~SchedulerWorker()
        {
            if (joinable())
            {
                // this is here just for
                // safety reasons, join should be called
                // from the owner scheduler.
                join();
            }

            // only in the destructor is cleaned up
            // the paused task container.
            //
            // at this point all the other workers are done,
            // and nobody can resume a paused task (at least not from this scheduler...)
            // so we can clean up here safely
            _Cleanup(_pausedTasks);
        }

        void join() { _thread.join(); }

        [[nodiscard]] auto joinable() const noexcept { return _thread.joinable(); }

    private:
        void Run(std::stop_token stopToken)
        {
            while (stopToken.stop_requested() == false)
            {
                Task_t task;
                if (_sharedTasks.try_pop(task) == false)
                {
                    // if we could not pop an element
                    // from the queue, we try to upload
                    // some cached tasks
                    _TryToUploadCachedTasks();

                    if (_cachedTasks.empty())
                    {
                        // the cache is empty, so we can
                        // wait safely for new tasks...
                        _sharedTasks.wait_for_pop();
                        continue;
                    }
                    else
                    {
                        // get the task from the cache
                        task.reset(_cachedTasks.pop());
                    }
                }
                else
                {
                    // most likely this is a good timepoint
                    // to upload some cached tasks...
                    _TryToUploadCachedTasks();
                }

                if (task == helper::SCHEDULER_STOP_EVENT)
                {
                    // if we popped the stop event
                    // out from the queue,
                    // we need to put back for other workers
                    helper::RequestStopForQueue(_sharedTasks);

                    // stop was requested we exit
                    // from the working loop
                    break;
                }
                else
                {
                    // Invoke the task.
                    // wrapping the task into a Task_t
                    // to make sure, there is a  proper destruction
                    _InvokeTask(std::move(task));
                }
            }

            // clean up all the local cached tasks
            // this can be done asynchronously...
            _Cleanup(_cachedTasks);
        }

        // Generates the pause resume callback
        // It relays on a task pointer address
        PauseHandlerCallbackT GeneratePauseResume(auto taskPtr)
        {
            return [this, taskPtr]() {
                if (_stopToken.stop_requested() == false)
                {
                    auto expected = taskPtr->pauseState.load(std::memory_order_relaxed);
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
                    helper::PushTask(Task_t{taskPtr}, _sharedTasks, _stopToken);
                }
            };
        }

        void _InvokeTask(Task_t task)
        {
            // sets the corrent pause resume callback
            // before any resumption
            task->SetPauseHandler(GeneratePauseResume(task.get()));

            using enum ETaskResumeState;
            while (_stopToken.stop_requested() == false)
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
                        if (_sharedTasks.try_push(std::move(task)))
                        {
                            // push succeed
                            // we simply return
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

                    // we can continue with the current task
                    // it is already notified for resumption
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
                auto taskPtr = _cachedTasks.pop();

                assert(taskPtr != nullptr);
                assert(taskPtr->next == nullptr);

                Task_t task{taskPtr};
                if (_sharedTasks.try_push(std::move(task)) == false)
                {
                    // If the push fails,
                    // we stop and exit.
                    //
                    // The task is pushed back to the front of the cache
                    // to help preserve the task order at least partially...
                    _cachedTasks.push_front(task.release());
                    break;
                }
            }
        }

        void _Cleanup(auto& tasks)
        {
            // iterates over the elements
            // and destroys it
            auto it = tasks.begin();
            while (it != nullptr)
            {
                auto  next = it->next;
                Task_t destroyer{it};
                it = next;
            }
        }

        // the queue which contains all the active tasks
        QueueT& _sharedTasks;

        // mutext to protect the paused tasks map
        std::mutex _pausedTasksMtx;

        // tasks which are in pause state
        detail::LinkedPtrList<typename Task_t::element_type> _pausedTasks;

        // Cache for tasks which could not be push back
        // immediately into the shared task queue.
        //
        // It's intented to mimic the basic task rotation
        // even with a full shared tasks queue.
        detail::LinkedPtrQueue<typename Task_t::element_type> _cachedTasks;

        // The scheduler stop token
        std::stop_token _stopToken;

        // The underlying worker thread
        std::jthread _thread;
    };

}} // namespace tinycoro::detail

#endif //!__TINY_CORO_SCHEDULER_WORKER_HPP__