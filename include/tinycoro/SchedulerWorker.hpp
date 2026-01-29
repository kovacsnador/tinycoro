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

    namespace local {

        // A minimal thread-safe wrapper around a LinkedPtrList.
        //
        // ThreadSafeList<T> provides a small set of atomic operations used by
        // SchedulerWorker for managing paused tasks. The wrapper protects the
        // underlying list with a mutex. Only the operations required by the
        // scheduler are exposed: iterating (begin), inserting at front and
        // erasing an element by pointer.
        template <typename T>
        struct ThreadSafeList
        {
            auto begin() const noexcept { return _list.begin(); }

            void insert(T* elem) noexcept
            {
                std::scoped_lock lock{_mtx};
                _list.push_front(elem);
            }

            bool erase(T* elem) noexcept
            {
                std::scoped_lock lock{_mtx};

                auto erased = _list.erase(elem);
                assert(erased);

                return erased;
            }

        private:
            // mutext to protect the list
            std::mutex _mtx;

            // List of elements
            detail::LinkedPtrList<T> _list;
        };

    } // namespace local

    template <typename DispatcherT>
    class SchedulerWorker
    {
        using Task_t = typename DispatcherT::value_type;

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
            while (stopToken.stop_requested() == false)
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
                    // Invoke the task.
                    // wrapping the task into a Task_t
                    // to make sure, there is a  proper destruction
                    _InvokeTask(std::move(task));
                }
            }

            // Not strictly necessary, but notify others
            // in case somebody missed the stop request
            _dispatcher.notify_all();

            // close the notified cache
            // and make a proper task cleanup here
            _Cleanup(_notifiedCachedTasks.close());

            // clean up all the local cached tasks
            // this can be done asynchronously...
            _Cleanup(_cachedTasks.begin());
        }

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

                if (self->_stopToken.stop_requested() == false)
                {
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
                        if (SharedStatePtr->CompareExchange(expected, desired, std::memory_order::release, std::memory_order::relaxed))
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
                    self->_pausedTasks.erase(promise);

                    // push back task to the queue for resumption
                    Task_t task{promise};
                    if (self->_stopToken.stop_requested() == false && policy != ENotifyPolicy::DESTROY)
                    {
                        // no stop was requested,
                        // and no immediate destroy policy.

                        // Save the dispatcher pointer for later use.
                        //
                        // This is not strictly necessary because tinycoro::ResumeCallback_t
                        // uses value semantics to pass its parameters (void* selfPtr and void* promisePtr).
                        auto dispatcherPtr = std::addressof(self->_dispatcher);

                        // After a successful push, we must return immediately.
                        //
                        // The task may resume and destroy itself before this function continues,
                        // which could lead to a heap use-after-free.
                        if (dispatcherPtr->try_push(std::move(task)) == false)
                        {
                            if (self->_notifiedCachedTasks.try_push(task.release()))
                            {
                                // wake up waiters, in case we are waiting for pop
                                dispatcherPtr->notify_all();
                            }
                            else
                            {
                                // The _notifiedCachedTasks stack is closed.
                                // Reassign the raw pointer to the RAII wrapper for proper destruction.
                                task.reset(promise);
                            }
                        }
                    }
                }
            };

            return {callback, this, promisePtr};
        }

        inline void _InvokeTask(Task_t task) noexcept
        {
            // sets the corrent pause resume callback
            // before any resumption
            task->SetResumeCallback(GeneratePauseResume(task.get()));

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
                    auto sharedStatePtr = task->SharedState();
                    auto expected       = sharedStatePtr->Load(std::memory_order::relaxed);

                    // state cannot be in paused
                    assert((expected & UTypeCast(EPauseState::PAUSED)) == 0);

                    if ((expected & UTypeCast(EPauseState::NOTIFIED)) == 0)
                    {
                        // no bit are set
                        assert((expected & UTypeCast(EPauseState::IDLE)) == 0);

                        // task is not notified yet,
                        // so we try to pause it.
                        auto promisePtr = task.release();

                        // push back into the pause state
                        _pausedTasks.insert(promisePtr);

                        while((expected & UTypeCast(EPauseState::NOTIFIED)) == 0)
                        {
                            // we try to set the PAUSED flag until the NOTIFIED
                            // is not set.

                            // decltype is necessary becasue of integer promotion
                            decltype(expected) desired = expected | UTypeCast(EPauseState::PAUSED);
                            if (sharedStatePtr->CompareExchange(expected, desired, std::memory_order::release, std::memory_order::relaxed))
                            {
                                // the task is in the paused task list
                                // we can return
                                return;
                            }
                        }

                        // This failed once, how????????
                        assert(expected & UTypeCast(EPauseState::NOTIFIED));
                        assert((expected & UTypeCast(EPauseState::PAUSED)) == 0);

                        // in the meantime the task is notified for resumption
                        // so we need to remove it from the paused task queue
                        // and resume the task
                        _pausedTasks.erase(promisePtr);

                        // reassign the pointer
                        // and continue with this task
                        task.reset(promisePtr);
                    }

                    // we can continue with the current task
                    // it is already notified for resumption
                    break;
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

        // the task dispatcher
        DispatcherT& _dispatcher;

        // tasks which are in pause state
        local::ThreadSafeList<typename Task_t::element_type> _pausedTasks;

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