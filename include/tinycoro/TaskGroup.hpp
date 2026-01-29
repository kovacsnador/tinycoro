// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License � see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_TASK_GROUP_HPP
#define TINY_CORO_TASK_GROUP_HPP

#include <mutex>
#include <tuple>
#include <stop_token>
#include <atomic>
#include <cassert>
#include <functional>
#include <memory>

#include "Common.hpp"
#include "UnsafeFuture.hpp"
#include "Wait.hpp"
#include "SchedulableTask.hpp"
#include "ResumeSignalEvent.hpp"
#include "Finally.hpp"
#include "LinkedPtrQueue.hpp"
#include "LinkedPtrList.hpp"
#include "AwaiterHelper.hpp"

namespace tinycoro {
    namespace detail {

        template <typename FutureT>
        struct TaskGroupBlock : detail::DoubleLinkable<TaskGroupBlock<FutureT>>
        {
            using callback_t = std::function<std::unique_ptr<TaskGroupBlock>(TaskGroupBlock*, bool)>;

            std::unique_ptr<TaskGroupBlock> OnFinish(bool isCancelled) noexcept
            {
                assert(notifyTaskGroup);

                return notifyTaskGroup(this, isCancelled);
            }

            callback_t notifyTaskGroup{};
            FutureT    future{};
        };

        template <typename UserDataT>
        struct TaskFinishCallback
        {
            template <typename PromiseT, typename FutureStateT>
            [[nodiscard]] static constexpr auto Get() noexcept
            {
                return [](void* promise, void* futureState, std::exception_ptr ex) {
                    auto p = static_cast<PromiseT*>(promise);
                    auto a = static_cast<UserDataT*>(p->CustomData());

                    // checking if the task got cancelled.
                    // if the task is cancelled we will ignore them
                    auto handle        = std::coroutine_handle<PromiseT>::from_promise(*p);
                    bool taskCancelled = (!handle.done() && !ex);

                    // we need to call OnFinish first,
                    // because this function is responsible to
                    // free up the memory, and this needs to happen
                    // before we set the future, becasue we wait for the future in the destructor.
                    [[maybe_unused]] std::unique_ptr<UserDataT> uPtr = a->OnFinish(taskCancelled);

                    // if the task is cancelled
                    // we need to get back the unique_ptr.
                    if (taskCancelled)
                        assert(uPtr);

                    if (taskCancelled == false)
                    {
                        // Call the default task finish handler to set the future.
                        detail::OnTaskFinish<PromiseT, FutureStateT>(promise, futureState, std::move(ex));
                    }
                };
            }
        };

        template <typename TaskGroupT, typename FutureT, typename FutureReturnT, typename EventT>
        struct NextAwaiter : public detail::SingleLinkable<NextAwaiter<TaskGroupT, FutureT, FutureReturnT, EventT>>
        {
            NextAwaiter(TaskGroupT& taskGroup)
            : _taskGroup{taskGroup}
            {
            }

            // disable move and copy
            NextAwaiter(NextAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept { return _taskGroup._IsReady(this); }

            [[nodiscard]] constexpr auto await_suspend(auto parentCoro) noexcept
            {
                _event.Set(context::PauseTask(parentCoro));

                auto suspended = _taskGroup._Suspend(this);

                if (suspended == false)
                {
                    // unpause if we need to resume already
                    context::UnpauseTask(parentCoro);
                }

                return suspended;
            }

            [[nodiscard]] constexpr FutureReturnT await_resume()
            {
                using futureValue_t = decltype(tinycoro::GetAll(_future));

                if constexpr (std::same_as<futureValue_t, void>)
                {
                    // in case we would return void, we simply
                    // return a bool to indicate if the awaiter
                    // was notified as regularly with a simple task finish.
                    if (_future.valid())
                    {
                        tinycoro::GetAll(_future);

                        // return a std::optional<VoidType>
                        return FutureReturnT{std::in_place};
                    }

                    // no result, closed
                    return std::nullopt;
                }
                else
                {
                    if (_future.valid())
                    {
                        return tinycoro::GetAll(_future);
                    }

                    // in this case it is cancelled
                    return std::nullopt;
                }
            }

            void Notify() noexcept { _event.Notify(ENotifyPolicy::RESUME); }

            void Set(FutureT&& future) noexcept { _future = std::move(future); }

        private:
            TaskGroupT& _taskGroup;

            EventT  _event;
            FutureT _future{};
        };

        template <typename TaskGroupT, typename FutureT, typename EventT>
        struct JoinAwaiter : public detail::SingleLinkable<JoinAwaiter<TaskGroupT, FutureT, EventT>>
        {
            JoinAwaiter(TaskGroupT& taskGroup)
            : _taskGroup{taskGroup}
            {
            }

            // disable move and copy
            JoinAwaiter(JoinAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept { return _taskGroup._IsReady(this); }

            [[nodiscard]] constexpr auto await_suspend(auto parentCoro) noexcept
            {
                _event.Set(context::PauseTask(parentCoro));

                auto suspend = _taskGroup._Suspend(this);
                if (suspend == false)
                {
                    // no suspend here
                    _event.Set(nullptr);
                    context::UnpauseTask(parentCoro);
                }

                return suspend;
            }

            // only the first join awaiter gets the results
            constexpr void await_resume() const noexcept { }

            void Notify() noexcept { _event.Notify(ENotifyPolicy::RESUME); }

        private:
            EventT      _event;
            TaskGroupT& _taskGroup;
        };

        template <typename ReturnT, template <typename> class FutureStateT>
        class TaskGroup
        {
            using futureTypeGetter_t = detail::FutureTypeGetter<ReturnT, FutureStateT>;

            using future_t       = futureTypeGetter_t::future_t;
            using futureReturn_t = futureTypeGetter_t::futureReturn_t;
            using block_t        = TaskGroupBlock<future_t>;

            friend struct NextAwaiter<TaskGroup, future_t, futureReturn_t, detail::ResumeSignalEvent>;
            using nextAwaiter_t = NextAwaiter<TaskGroup, future_t, futureReturn_t, detail::ResumeSignalEvent>;

            friend struct JoinAwaiter<TaskGroup, future_t, detail::ResumeSignalEvent>;
            using joinAwaiter_t = JoinAwaiter<TaskGroup, future_t, detail::ResumeSignalEvent>;

            using stopCallback_t = std::optional<std::stop_callback<std::function<void()>>>;

        public:
            using value_type = ReturnT;

            TaskGroup() = default;

            // Stop source as dependency injection
            TaskGroup(std::stop_source ss)
            {
                // delegate the parent stop source to the child tasks.
                _stopCallback.emplace(ss.get_token(), [this] {
                    assert(_stopSource.stop_possible());
                    _stopSource.request_stop();
                });
            }

            // disallow copy and move
            TaskGroup(TaskGroup&&) = delete;

            // Destroying a TaskGroup waits for all
            // tasks to finish and suppresses exceptions.
            ~TaskGroup()
            {
                // After Join(), no tasks are running and no further Finish-callbacks can occur.
                auto joinAll = [this]() -> InlineTask<> { co_await Join(); };
                tinycoro::AllOf(joinAll());

                std::unique_lock lock{_mtx};

                assert(_closed);
                assert(_runningTaskblocks.empty());

                // clean up all the unused _readyFutureBlocks
                auto awaiter = _awaitReadyBlocks.steal();
                lock.unlock();

                // iterate over the remaining ready block
                // and destroy them.
                while (std::unique_ptr<block_t> block{awaiter})
                {
                    awaiter = block->next;
                }
            }

            // Spawns a new task into the task group and schedules it on the given scheduler.
            //
            // The task must have the same `value_type` as the TaskGroup. Ownership of the task
            // is transferred to the TaskGroup. The TaskGroup propagates its stop token to the task.
            //
            // If the TaskGroup is already closed (via Close(), Join(), or CancelAll()),
            // the task will not be scheduled and the function returns false.
            //
            // Thread-safe.
            template <typename SchedulerT, concepts::IsCorouitneTask TaskT>
            bool Spawn(SchedulerT& scheduler, TaskT&& task)
            {
                static_assert(!std::is_reference_v<TaskT>, "Task must be passed as an rvalue (do not use a reference).");
                static_assert(std::same_as<typename TaskT::value_type, value_type>,
                              "Return value mismatch! Task type is incompatible with TaskGroup.");

                auto block = std::make_unique<block_t>();

                block->notifyTaskGroup = [this](auto blockPtr, bool isCancelled) { return _OnTaskFinish(blockPtr, isCancelled); };

                // prepare the tasks as they are only
                // stop token users, but they self cannot trigger
                // a stop.
                task.SetCustomData(block.get());
                task.SharedState()->MarkStopTokenUser();
                task.SetStopSource(_stopSource);

                using finishCallback_t = TaskFinishCallback<block_t>;

                std::scoped_lock lock{_mtx};

                if (_closed == false)
                {
                    auto blockPtr = block.release();

                    // take ownership over the block the _runningTaskblocks
                    _runningTaskblocks.push_front(blockPtr);

                    // here we can check if the taskgroup is closed,
                    blockPtr->future = scheduler.template Enqueue<FutureStateT, finishCallback_t>(std::move(task));
                }

                // adding task was successful if
                // the state is not closed.
                return !_closed;
            }

            // Cancels all running tasks in the group.
            //
            // This function closes the TaskGroup and requests cancellation via its stop source.
            // Tasks that observe the stop token may terminate early.
            //
            // After calling CancelAll(), spawning new tasks is not allowed.
            //
            // Thread-safe.
            void CancelAll() noexcept
            {
                Close();
                assert(_stopSource.stop_possible());

                _stopSource.request_stop();
            }

            // Closes the TaskGroup.
            //
            // After closing, no new tasks can be spawned. Already running tasks continue
            // executing until completion or cancellation.
            //
            // Any awaiters waiting on Next() will be notified if no further results
            // can become available.
            //
            // Thread-safe.
            void Close() noexcept
            {
                std::unique_lock lock{_mtx};

                _closed = true;

                // we need to notify the currently waiting
                // next awaiters.
                auto nextAwaiters = _nextAwaiters.steal();
                lock.unlock();

                _NotifyAll(nextAwaiters);
            }

            // Returns an awaiter that yields the next completed task result.
            //
            // The awaiter suspends the caller until a task finishes or the TaskGroup
            // becomes closed and empty.
            //
            // The result is returned as `std::optional<value_type>`:
            // - contains a value if a task completed successfully,
            // - empty if the TaskGroup is closed and no further results exist.
            //
            // Multiple concurrent Next() awaiters are supported.
            //
            // Thread-safe.
            [[nodiscard]] auto Next() noexcept -> nextAwaiter_t { return nextAwaiter_t{*this}; }

            // Attempts to retrieve the next completed task result without suspension.
            //
            // If a completed task is available, its result is returned immediately.
            // Otherwise, an empty `std::optional` is returned.
            //
            // This function never suspends and does not block.
            //
            // Thread-safe.
            //
            // Returns std::optional containing the next task result if available, otherwise empty.
            [[nodiscard]] auto TryNext() noexcept
            {
                std::unique_lock lock{_mtx};

                if (std::unique_ptr<block_t> block{_awaitReadyBlocks.pop()})
                {
                    lock.unlock();

                    if constexpr (std::same_as<void, ReturnT>)
                    {
                        tinycoro::GetAll(block->future);

                        // return a std::optional<VoidType>
                        return futureReturn_t{std::in_place};
                    }
                    else
                    {
                        return tinycoro::GetAll(block->future);
                    }
                }

                // no task left
                return futureReturn_t{};
            }

            // Returns an awaiter that completes when all tasks in the group have finished.
            //
            // Calling Join() implicitly closes the TaskGroup, preventing further task spawning.
            //
            // Multiple Join() awaiters are allowed, but only the first one observes the
            // completion state directly; others are simply resumed.
            //
            // Thread-safe.
            [[nodiscard]] auto Join() noexcept -> joinAwaiter_t { return joinAwaiter_t{*this}; }

            // Returns the stop source associated with this TaskGroup.
            //
            // The stop source is propagated to all spawned tasks, allowing cooperative
            // cancellation.
            //
            // Thread-safe.
            [[nodiscard]] auto StopSource() noexcept { return _stopSource; }

        private:
            template <typename T>
            void _NotifyAll(T* awaiter)
            {
                // Notify all waiters
                detail::IterInvoke(awaiter, &T::Notify);
            }

            [[nodiscard]] bool _IsDone() const noexcept { return (_closed && _awaitReadyBlocks.empty() && _runningTaskblocks.empty()); }

            [[nodiscard]] std::unique_ptr<block_t> _OnTaskFinish(block_t* readyBlock, bool isCancelled) noexcept
            {
                assert(readyBlock);

                std::unique_ptr<block_t> block{readyBlock};

                std::unique_lock lock{_mtx};

                [[maybe_unused]] auto erased = _runningTaskblocks.erase(block.get());

                assert(erased);
                assert(block->future.valid());

                // in case the task is cancelled, we don't touch the next awaiters list
                auto awaiter = isCancelled ? nullptr : _nextAwaiters.pop();

                joinAwaiter_t* joinAwaiters{nullptr};
                nextAwaiter_t* nextAwaiters{nullptr};

                // check if we are done
                if (_closed && _runningTaskblocks.empty())
                {
                    // there are no tasks which needs to be waited
                    // and we are in the close state.
                    //
                    // So we notify here all the join awaiters.
                    joinAwaiters = _joinAwaiters.steal();
                    nextAwaiters = _nextAwaiters.steal();
                }

                if (awaiter)
                {
                    lock.unlock();

                    awaiter->Set(std::move(block->future));
                    awaiter->Notify();
                }
                else
                {
                    if (isCancelled == false)
                    {
                        // in case the task is NOT cancelled and
                        // no awaiter waiting for the task
                        // save that for the future
                        _awaitReadyBlocks.push(block.release());
                    }

                    lock.unlock();
                }

                assert(lock.owns_lock() == false);

                // in case we need somebody to notify.
                _NotifyAll(nextAwaiters);
                _NotifyAll(joinAwaiters);

                // pass the block back to the caller
                // and he can decide if he want to keep them
                // or destroy it.
                //
                // We simply delay the destruction and pass the ownership
                // back the the caller.
                return block;
            }

            // for NextAwaiter
            [[nodiscard]] bool _IsReady(nextAwaiter_t* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                // if the group is closed
                // and there are no running or unconsumed tasks
                // we are done.
                if (_IsDone())
                {
                    return true;
                }

                if (std::unique_ptr<block_t> block{_awaitReadyBlocks.pop()})
                {
                    // await is ready
                    awaiter->Set(std::move(block->future));
                    return true;
                }

                // await is not ready, probably
                // pass forward to await_suspend
                return false;
            }

            [[nodiscard]] auto _Suspend(nextAwaiter_t* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                if (_IsDone())
                {
                    // if the group is closed
                    // and there are no running or unconsumed tasks
                    // we are done.
                    return false;
                }

                if (std::unique_ptr<block_t> block{_awaitReadyBlocks.pop()})
                {
                    awaiter->Set(std::move(block->future));
                    return false;
                }

                _nextAwaiters.push(awaiter);
                return true;
            }

            // for JoinAwaiter
            [[nodiscard]] auto _IsReady([[maybe_unused]] joinAwaiter_t* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                _closed = true;

                // if the task group is closed,
                // and we have no running futures
                // everything is done.
                return _runningTaskblocks.empty();
            }

            [[nodiscard]] auto _Suspend(joinAwaiter_t* awaiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                assert(_closed);

                if (_runningTaskblocks.empty())
                {
                    // if the task group is closed,
                    // and we have no running futures
                    // everything is done.
                    // No suspend necessary.
                    return false;
                }

                _joinAwaiters.push(awaiter);
                return true;
            }

            std::mutex _mtx;

            // Close flag.
            // If it set to true spawning new task is a no-op.
            // No incomming task will be excepted.
            bool _closed{false};

            std::stop_source _stopSource{};
            stopCallback_t   _stopCallback;

            // Queue storing currently running task blocks (not yet ready).
            //
            // These blocks are heap-allocated, so
            // we are responsible for freeing them.
            detail::LinkedPtrList<block_t> _runningTaskblocks;

            // Queue storing await-ready blocks.
            //
            // These blocks are heap-allocated, so
            // we are responsible for freeing them.
            detail::LinkedPtrQueue<block_t> _awaitReadyBlocks;

            // Awaiters waiting for the next task completion.
            detail::LinkedPtrQueue<nextAwaiter_t> _nextAwaiters;
            // Awaiters waiting for all tasks to complete (join).
            detail::LinkedPtrQueue<joinAwaiter_t> _joinAwaiters;
        };

    } // namespace detail

    template <typename ReturnT = void>
    using TaskGroup = detail::TaskGroup<ReturnT, unsafe::Promise>;

    // Blocks the current thread until all tasks in the TaskGroup have finished.
    //
    // This function is a synchronous wrapper around TaskGroup::Join().
    // It closes the TaskGroup and waits for completion of all tasks.
    template <typename T>
    void Join(TaskGroup<T>& taskGroup) noexcept
    {
        auto joinBlocking = [&taskGroup]() -> tinycoro::InlineTask<> { co_await taskGroup.Join(); };
        tinycoro::AllOf(joinBlocking());
    }

} // namespace tinycoro

#endif // TINY_CORO_TASK_GROUP_HPP