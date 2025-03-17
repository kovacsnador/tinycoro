#ifndef __TINY_CORO_SCHEDULER_WORKER_HPP__
#define __TINY_CORO_SCHEDULER_WORKER_HPP__

#include <thread>
#include <mutex>

#include "LinkedPtrQueue.hpp"
#include "Common.hpp"

namespace tinycoro { namespace detail {
    template <typename TaskT>
    class SchedulerWorker
    {
    public:
        using thief_t = std::function<typename TaskT::element_type*()>;
        using global_notifier_t = std::function<void()>;

        SchedulerWorker(std::stop_token stopToken, thief_t taskThief, global_notifier_t notifier)
        : _stopToken{stopToken}
        , _taskThief{std::move(taskThief)}
        , _notifier{std::move(notifier)}
        , _thread{[this](std::stop_token st) { Run(st); }, stopToken}
        {
        }

        void join()
        {
            // join the thread
            _thread.join();

            // cleans up every task
            _CleanUpDanglingTasks();
        }

        auto joinable() { return _thread.joinable(); }

        void Push(TaskT& task)
        {
            task->SetPauseHandler(GeneratePauseResume(task.get()));

            {
                std::scoped_lock lock{_tasksQueueMtx};
                _tasks.push(task.release());
            }

            _cv.notify_all();
            _notifier();
        }

        auto Pop()
        {
            std::scoped_lock lock{_tasksQueueMtx};
            return _tasks.pop();
        }

        void Notify() { _cv.notify_all(); }

    private:
        void Run(std::stop_token stopToken)
        {
            while (stopToken.stop_requested() == false)
            {
                typename TaskT::element_type* ta{nullptr};

                // wait on conditional variable
                std::unique_lock queueLock{_tasksQueueMtx};
                /*if (_cv.wait(queueLock, stopToken, [this, &ta, &queueLock] {
                    queueLock.unlock(); 
                    ta = _taskThief();
                    queueLock.lock();
                    return !_tasks.empty() || ta != nullptr; }) == false)
                {
                    // stop was requested
                    return;
                }*/

                if (_cv.wait(queueLock, stopToken, []{ return true; }) == false)
                {
                    // stop was requested
                    return;
                }

                while (stopToken.stop_requested() == false && _tasks.empty() == false)
                {
                    {
                        TaskT task{_tasks.pop()};

                        queueLock.unlock();

                        _InvokeTask(task);
                    }

                    queueLock.lock();
                }

                queueLock.unlock();

                ta = _taskThief();

                while (stopToken.stop_requested() == false && ta)
                {
                    {
                        TaskT task{ta};
                        task->ResetPauseHandler(GeneratePauseResume(task.get()));

                        _InvokeTask(task);
                    }

                    // try to get the next task for the other worker
                    ta = _taskThief();
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

                    _notifier();
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
                    _notifier();

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

        // mutext to protect the active tasks queue
        std::mutex _tasksQueueMtx;

        // mutext to protect the paused tasks map
        std::mutex _pausedTasksMtx;

        // currently active/scheduled tasks
        LinkedPtrQueue<typename TaskT::element_type> _tasks;

        // tasks which are in pause state
        detail::LinkedPtrList<typename TaskT::element_type> _pausedTasks;

        // conditional variable used to notify
        // if we have active tasks in the queue
        std::condition_variable_any _cv;

        std::stop_token _stopToken;

        thief_t _taskThief;

        global_notifier_t _notifier;

        std::jthread _thread;
    };

}} // namespace tinycoro::detail

#endif //!__TINY_CORO_SCHEDULER_WORKER_HPP__