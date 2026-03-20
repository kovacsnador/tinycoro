// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SCHEDULER_WORKER_HPP
#define TINY_CORO_SCHEDULER_WORKER_HPP

#include <mutex>
#include <cstddef>

#include "LinkedPtrQueue.hpp"
#include "AtomicPtrStack.hpp"
#include "Common.hpp"
#include "Finally.hpp"

namespace tinycoro { namespace detail {

    template <typename DispatcherT>
    class SchedulerWorker
    {
        using Task_t = typename DispatcherT::value_type;

    public:
        explicit SchedulerWorker(DispatcherT& dispatcher)
        : _dispatcher{dispatcher}
        {
        }

        // disable copy and move
        SchedulerWorker(SchedulerWorker&&) = delete;

        // Drains and executes queued tasks from
        // the dispatcher until the queue becomes empty
        void DrainQueuedTasks() noexcept
        {
            while (_dispatcher.task_counter())
            {
                Task_t task{};

                auto popState = _dispatcher.pop_state();
                if (_dispatcher.try_pop(task))
                {
                    // Invoke the task.
                    // wrapping the task into a Task_t
                    // to make sure, there is a  proper destruction
                    _InvokeTask(std::move(task));
                }
                else
                {
                    // we could not pop active task from the queue
                    // but we still have some paused task(s) which
                    // they are waiting for resumption.
                    _dispatcher.wait_for_pop(popState);
                }
            }

            // should be always empty
            assert(_notifiedCachedTasks.empty());
            assert(_cachedTasks.empty());
        }

        void Run(std::stop_token stopToken) noexcept
        {
            for (;;)
            {
                // we can try to upload the cached tasks
                Task_t task = _TryToUploadCachedTasks();

                if (task == nullptr && _dispatcher.try_pop(task) == false)
                {
                    assert(_cachedTasks.empty());

                    // Get the pop state before we check _notifiedCachedTasks.
                    // This is important becasue it could happen, that a task is
                    // landing in the _notifiedCachedTasks in the mean time
                    // and we don't want to miss the notification "dispatcher.notify_all()"
                    auto popState = _dispatcher.pop_state();
                    if (_notifiedCachedTasks.empty())
                    {
                        if (stopToken.stop_requested() && _dispatcher.task_counter() == 0)
                        {
                            assert(_cachedTasks.empty());
                            assert(_notifiedCachedTasks.empty());

                            // scheduler requested stop...
                            // There are no tasks in the dispatcher
                            // so we are done 
                            break;
                        }

                        // all the caches are empty, we can
                        // wait safely for new tasks...
                        //
                        // now if some tasks need resumption
                        // they will directly be pushed into the dispatcher queue.
                        // (not in the local cache)
                        _dispatcher.wait_for_pop(popState);
                    }
                }
                else
                {
                    // Resume the task
                    _InvokeTask(std::move(task));
                }
            }

            // Not strictly necessary, but notify others
            // in case somebody missed the stop request
            _dispatcher.notify_all();

            // should be always empty
            assert(_notifiedCachedTasks.empty());
            assert(_cachedTasks.empty());
            assert(_dispatcher.task_counter() == 0);
        }

    private:
        // Generates the pause resume callback
        // It relays on a task pointer address
        template <typename PromiseT>
        ResumeCallback_t GeneratePauseResume(PromiseT promisePtr) noexcept
        {
            using self_t = decltype(this);

            auto callback = [](void* selfPtr, void* promisePtr, ENotifyPolicy policy) {
                // worker pointer
                auto self = static_cast<self_t>(selfPtr);
                // promise poiner
                auto promise = static_cast<PromiseT>(promisePtr);

                auto SharedStatePtr = promise->SharedState();
                auto expected       = SharedStatePtr->Load(std::memory_order::acquire);

                while ((expected & UTypeCast(EPauseState::PAUSED)) == 0)
                {
                    // we try to set the NOTIFIED flag until the PAUSED
                    // is not set.

                    // decltype is necessary becasue of integer promotion
                    decltype(expected) desired = expected | UTypeCast(EPauseState::NOTIFIED);

                    // If the notify callback invoked very quickly
                    // we have here a little time window to tell to
                    // the scheduler, that the task is ready for resumption
                    if (SharedStatePtr->CompareExchange(expected, desired, std::memory_order::relaxed, std::memory_order::relaxed))
                    {
                        // The task is notified
                        // in time, so we are done.
                        return;
                    }
                }

                assert(expected & UTypeCast(EPauseState::PAUSED));
                assert((expected & UTypeCast(EPauseState::NOTIFIED)) == 0);

                // If we reach this point
                // that means that the task is already in paused state
                // So we need to resume it manually

                // push back task to the queue for resumption
                Task_t task{promise};

                if (policy == ENotifyPolicy::DESTROY)
                {
                    // immediate destroy policy
                    // this task is cancelled, so we can simply throw
                    // it away
                    self->_dispatcher.decrease_task_counter(1);
                    return;
                }

                // After a successful push, we must return immediately.
                //
                // The task may resume and destroy itself before this function continues,
                // which could lead to a heap use-after-free.
                if (self->_dispatcher.try_push(std::move(task)) == false)
                {
                    if (self->_notifiedCachedTasks.try_push(task.release()))
                    {
                        // wake up waiters, in case we are waiting for pop
                        self->_dispatcher.notify_all();
                    }
                    else
                    {
                        // should be never happen.
                        assert(false);

                        // The _notifiedCachedTasks stack is closed.
                        // Reassign the raw pointer to the RAII wrapper for proper destruction.
                        task.reset(promise);
                    }
                }

                // after _dispatcher->try_push succeed, we need to resume immediately
                // because this callback function can be destroyed.
            };

            return {callback, this, promisePtr};
        }

        inline void _InvokeTask(Task_t task) noexcept
        {
            // sets the corrent pause resume callback
            // before any resumption
            task->SetResumeCallback(GeneratePauseResume(task.get()));

            using enum ETaskResumeState;
            for (;;)
            {
                // resume the task and
                // get the resume state from the
                // coroutine (or from his continuation)
                auto resumeState = task->Resume();

                switch (resumeState)
                {
                case SUSPENDED: {

                    // push back the task into the queue
                    //
                    // here potentially we could also just
                    // continue the execution of the task...
                    if (_dispatcher.try_push(std::move(task)) == false)
                    {
                        // the queue is full
                        // so we are saving this task,
                        // and trying to push back into the
                        // shared tasks queue later
                        //
                        // alternatively we could just continue here
                        // with the current promisePtr execution
                        _cachedTasks.push(task.release());
                    }
                    return;
                }
                case PAUSED: {
                    auto sharedStatePtr = task->SharedState();
                    auto expected       = sharedStatePtr->Load(std::memory_order::relaxed);

                    // state cannot be in paused
                    assert((expected & UTypeCast(EPauseState::PAUSED)) == 0);

                    while ((expected & UTypeCast(EPauseState::NOTIFIED)) == 0)
                    {
                        // we try to set the PAUSED flag until the NOTIFIED
                        // is not set.
                        // decltype is necessary becasue of integer promotion
                        decltype(expected) desired = expected | UTypeCast(EPauseState::PAUSED);
                        if (sharedStatePtr->CompareExchange(expected, desired, std::memory_order::release, std::memory_order::relaxed))
                        {
                            // the task is in the paused task list
                            // we can return
                            std::ignore = task.release();
                            return;
                        }
                    }

                    assert(expected & UTypeCast(EPauseState::NOTIFIED));
                    assert((expected & UTypeCast(EPauseState::PAUSED)) == 0);

                    // we can continue with the current task
                    // it is already notified for resumption
                    break;
                }
                case STOPPED:
                    [[fallthrough]];
                case DONE:
                    [[fallthrough]];
                default:
                    _dispatcher.decrease_task_counter(1);
                    return;
                }
            }
        }

        [[nodiscard]] auto _TryToUploadCachedTasks() noexcept
        {
            // copy notified task to the cached tasks
            auto promisePtr = _notifiedCachedTasks.steal();
            while (promisePtr)
            {
                auto next        = promisePtr->next;
                promisePtr->next = nullptr;
                _cachedTasks.push(promisePtr);
                promisePtr = next;
            }

            // get the first (oldest) task and return it from this function
            auto promise = _cachedTasks.pop();

            while (_cachedTasks.empty() == false)
            {
                // get the first task from the cache
                promisePtr = _cachedTasks.pop();

                assert(promisePtr != nullptr);
                assert(promisePtr->next == nullptr);

                Task_t task{promisePtr};
                if (_dispatcher.try_push(std::move(task)) == false)
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

            return Task_t{promise};
        }

        // the task dispatcher
        DispatcherT& _dispatcher;

        // Cache for tasks which could not be push back
        // immediately into the shared task queue.
        //
        // It's intented to mimic the basic task rotation
        // even with a full shared tasks queue.
        detail::LinkedPtrQueue<typename Task_t::element_type> _cachedTasks;

        // These pending tasks are waiting to be resumed.
        //
        // The tasks were in a paused state and have already been notified for resumption.
        // However, there was no space in the sharedTask queue,
        // so we store them here to guarantee their continued execution.
        detail::AtomicPtrStack<typename Task_t::element_type> _notifiedCachedTasks;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_SCHEDULER_WORKER_HPP