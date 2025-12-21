// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SCHEDULER_WORKER_HPP
#define TINY_CORO_SCHEDULER_WORKER_HPP

#include <thread>
#include <mutex>
#include <cstddef>

#include "LinkedPtrQueue.hpp"
#include "AtomicPtrStack.hpp"
#include "Common.hpp"
#include "Finally.hpp"

namespace tinycoro { namespace detail {

    namespace helper {
        // With this variable we indicate that
        // a stop purposed by the scheduler
        static constexpr std::nullptr_t SCHEDULER_STOP_EVENT{nullptr};

        bool PushTask(auto task, auto& queue, const auto& stopObject, auto& popState) noexcept
        {
            while (stopObject.stop_requested() == false)
            {
                //auto prevState = queue.pop_state();

                if (queue.try_push(std::move(task)))
                {
                    // the task is pushed
                    // into the tasks queue
                    return true;
                }
                else
                {
                    // wait until we have space in the queue
                    queue.wait_for_push(popState.fetch_add(1, std::memory_order::release));
                }
            }

            return false;
        }

        template <typename DispatcherT>
        void RequestStopForQueue(DispatcherT& dispatcher) noexcept
        {
            // this is necessary to trigger/wake up
            // wait_for_push() waiters
            //
            // this should happen before we push
            // the SCHEDULER_STOP_EVENT into the queue,
            // because this could also remove the
            // SCHEDULER_STOP_EVENT from the queue if we invoke
            // after SCHEDULER_STOP_EVENT push...
            while (dispatcher.full())
            {
                typename DispatcherT::value_type destroyer{};
                if (dispatcher.try_pop(destroyer))
                {
                    // erase at least 1 element
                    break;
                }
            }

            // try to push the close event into the task dispatcher
            while (dispatcher.try_push(SCHEDULER_STOP_EVENT) == false)
            {
                // try to remove one element
                // in order to make place for
                // SCHEDULER_STOP_EVENT
                typename DispatcherT::value_type destroyer{};
                std::ignore = dispatcher.try_pop(destroyer);
            }
        }

    } // namespace helper

    template <typename DispatcherT>
    class SchedulerWorker
    {
        using Task_t = typename DispatcherT::value_type;

        //size_t doneCount{};

    public:
        SchedulerWorker(DispatcherT& dispatcher, std::stop_token stopToken)
        : _dispatcher{dispatcher}
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
            _Cleanup(_pausedTasks.begin());
        }

        void join() { _thread.join(); }

        [[nodiscard]] auto joinable() const noexcept { return _thread.joinable(); }

    private:
        void Run(std::stop_token stopToken) noexcept
        {
            typename DispatcherT::state_type prevState{};
            while (stopToken.stop_requested() == false)
            {
                // we can try to upload the cached tasks
                Task_t task = _TryToUploadCachedTasks();

                if (task == nullptr && _dispatcher.try_pop(task) == false)
                {
                    assert(_cachedTasks.empty());

                    if (_cachedTasks.empty() && _notifiedCachedTasks.empty())
                    {
                        // the all the caches are empty, we can
                        // wait safely for new tasks...
                        //
                        // now if some tasks need resumption
                        // they will directly be pushed into the sharedTasks queue.
                        // (not in the cache)
                        _dispatcher.wait_for_pop(prevState++);
                    }

                }
                else
                {
                    // If we reach that point, that means
                    // we successfully popped the next task
                    if (task == helper::SCHEDULER_STOP_EVENT)
                    {
                        // if we popped the stop event
                        // out from the queue,
                        // we need to put back for other workers
                        helper::RequestStopForQueue(_dispatcher);

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
            }

            // close the notified cache
            // and make a proper task cleanup here
            _Cleanup(_notifiedCachedTasks.close());

            // clean up all the local cached tasks
            // this can be done asynchronously...
            _Cleanup(_cachedTasks.begin());
        }

        //std::atomic_flag _resumer{false};

        // Generates the pause resume callback
        // It relays on a task pointer address
        PauseHandlerCallbackT GeneratePauseResume(auto promisePtr) noexcept
        {
            return [this, promisePtr](ENotifyPolicy policy) {

                /*if(_resumer.test_and_set() == true)
                {
                    assert(false);
                }

                auto finally = Finally([&]{ _resumer.clear(); });*/

                if (_stopToken.stop_requested() == false)
                {
                    auto expected = promisePtr->pauseState.load(std::memory_order_relaxed);
                    while (expected == EPauseState::IDLE)
                    {
                        // If the notify callback invoked very quickly
                        // we have here a little time window to tell to
                        // the scheduler, that the task is ready for resumption
                        if (promisePtr->pauseState.compare_exchange_weak(
                                expected, EPauseState::NOTIFIED, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // The task is notified
                            // in time, so we are done.
                            return;
                        }
                    }

                    assert(expected == EPauseState::PAUSED);

                    // If we reach this point
                    // that means that the task is already in paused state
                    // So we need to resume it manually

                    /*{
                        std::scoped_lock pauseLock{_pausedTasksMtx};
                        // remove the tasks from paused tasks
                        [[maybe_unused]] auto erased = _pausedTasks.erase(promisePtr);
                        assert(erased);
                    }*/
                    _EraseFromPausedList(promisePtr);


                    // push back task to the queue for resumption
                    Task_t task{promisePtr};
                    if (_stopToken.stop_requested() == false && policy != ENotifyPolicy::DESTROY)
                    {
                        // no stop was requested,
                        // and no immediate destroy policy.

                        // save dispatcher pointer for later use.
                        auto dispatcherPtr = std::addressof(_dispatcher);

                        // After a successful push, we must return immediately.
                        // The task may resume and destroy itself before this function continues,
                        // which could lead to a heap use-after-free.
                        if (dispatcherPtr->try_push(std::move(task)) == false)
                        {
                            if (_notifiedCachedTasks.try_push(task.release()))
                            {
                                // wake up waiters, in case we are waiting for pop
                                dispatcherPtr->notify_pop_waiters();
                            }
                            else
                            {
                                // The _notifiedCachedTasks stack is closed.
                                // Reassign the raw pointer to the RAII wrapper for proper destruction.
                                task.reset(promisePtr);
                            }
                        }
                    }
                }
            };
        }

        inline void _InvokeTask(Task_t task) noexcept
        {
            // sets the corrent pause resume callback
            // before any resumption
            task->SetPauseHandler(GeneratePauseResume(task.get()));

            using enum ETaskResumeState;
            while (_stopToken.stop_requested() == false)
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
                    if (_stopToken.stop_requested() == false)
                    {
                        // no stop was requested
                        if (_dispatcher.try_push(std::move(task)))
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
                        // with the current promisePtr execution
                        _cachedTasks.push(task.release());
                    }
                    return;
                }
                case PAUSED: {
                    auto expected = task->PauseState().load(std::memory_order_acquire);
                    assert(expected != EPauseState::PAUSED);

                    if (expected == EPauseState::IDLE)
                    {
                        auto promisePtr = task.release();

                        /*std::unique_lock pauseLock{_pausedTasksMtx};
                        // push back into the pause state
                        _pausedTasks.push_front(promisePtr);

                        pauseLock.unlock();*/
                        _InsertInPausedList(promisePtr);


                        if (promisePtr->pauseState.compare_exchange_strong(
                                expected, EPauseState::PAUSED, std::memory_order_release, std::memory_order_relaxed))
                        {
                            // the task is in the paused task list
                            // we can return
                            return;
                        }

                        /*pauseLock.lock();
                        // in the meantime the task is notified for resumption
                        // so we need to remove it from the paused task queue
                        // and resume the task
                        _pausedTasks.erase(promisePtr);

                        pauseLock.unlock();*/

                        _EraseFromPausedList(promisePtr);

                        // reassign the pointer
                        // and continue with this task
                        task.reset(promisePtr);
                    }

                    //std::this_thread::sleep_for(std::chrono::milliseconds(10));

                    // we can continue with the current task
                    // it is already notified for resumption
                    break;
                }
                case STOPPED:
                    [[fallthrough]];
                case DONE:
                    //doneCount++;
                    [[fallthrough]];
                default:
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

            while (_cachedTasks.empty() == false && _stopToken.stop_requested() == false)
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

        void _Cleanup(auto promisePtr) noexcept
        {
            // iterates over the elements
            // and destroys it
            while (promisePtr)
            {
                auto   next = promisePtr->next;
                Task_t destroyer{promisePtr};
                promisePtr = next;
            }
        }

        void _InsertInPausedList(auto promisePtr)
        {
            std::scoped_lock lock{_pausedTasksMtx};
            _pausedTasks.push_front(promisePtr);
        }

        void _EraseFromPausedList(auto promisePtr)
        {
            std::scoped_lock lock{_pausedTasksMtx};
            [[maybe_unused]] auto erased = _pausedTasks.erase(promisePtr);

            assert(erased);
        }

        // the task dispatcher
        DispatcherT& _dispatcher;

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

        // These pending tasks are waiting to be resumed.
        //
        // The tasks were in a paused state and have already been notified for resumption.
        // However, there was no space in the sharedTask queue,
        // so we store them here to guarantee their continued execution.
        detail::AtomicPtrStack<typename Task_t::element_type> _notifiedCachedTasks;

        // The scheduler stop token
        std::stop_token _stopToken;

        // The underlying worker thread
        std::jthread _thread;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_SCHEDULER_WORKER_HPP