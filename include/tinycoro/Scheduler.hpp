#ifndef __TINY_CORO_CORO_SCHEDULER_HPP__
#define __TINY_CORO_CORO_SCHEDULER_HPP__

#include <thread>
#include <future>
#include <functional>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <concepts>
#include <assert.h>
#include <ranges>
#include <algorithm>
#include <variant>

#include "Future.hpp"
#include "Common.hpp"
#include "PauseHandler.hpp"
#include "PackagedTask.hpp"

using namespace std::chrono_literals;

namespace tinycoro {

    template <std::move_constructible TaskT>
        requires requires (TaskT t) {
            { std::invoke(t) } -> std::same_as<ETaskResumeState>;
            { t.IsPaused() } -> std::same_as<bool>;
        }
    class CoroThreadPool
    {
        // Placeholder task type for to notify that the task should not be put on pause.
        struct DoNotPauseTask
        {
        };

        using TaskVariantT = std::variant<DoNotPauseTask, TaskT>;

    public:
        CoroThreadPool(size_t workerThreadCount) { _AddWorkers(workerThreadCount); }

        ~CoroThreadPool()
        {
            _stopSource.request_stop();
            std::ranges::for_each(_workerThreads, [](auto& it) {
                if (it.joinable())
                {
                    it.join();
                }
            });
            _tasksCount.store(0, std::memory_order::release);
        }

        // Disable copy and move
        CoroThreadPool(const CoroThreadPool&) = delete;
        CoroThreadPool(CoroThreadPool&&)      = delete;

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

        void Wait()
        {
            auto count = _tasksCount.load(std::memory_order_acquire);
            while (count > 0)
            {
                _tasksCount.wait(count, std::memory_order_acquire);
                count = _tasksCount.load(std::memory_order_acquire);
            }
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

            auto future = futureState.get_future();

            if (_stopSource.stop_requested() == false)
            {
                _tasksCount.fetch_add(1, std::memory_order_relaxed);

                auto id = _coroutineIdCount.fetch_add(1, std::memory_order_relaxed);
                coro.SetPauseHandler(GeneratePauseResume(id));

                {
                    std::scoped_lock lock{_tasksQueueMtx};
                    _tasks.emplace(std::move(coro), std::move(futureState), id);
                }

                _cv.notify_all();
            }

            return future;
        }

        PauseHandlerCallbackT GeneratePauseResume(cid_t id)
        {
            return [this, i = id]() {
                if (_stopSource.stop_requested() == false)
                {
                    std::unique_lock pauseLock{_pausedTasksMtx};

                    if (auto it = _pausedTasks.find(i); it != _pausedTasks.end())
                    {
                        std::scoped_lock queueLock{_tasksQueueMtx};

                        _tasks.emplace(std::move(std::get<TaskT>(it->second)));
                        _pausedTasks.erase(i);

                        // pause lock can be released
                        pauseLock.unlock();

                        // if we notify, we still need to hold the lock.
                        // otherwise other threads can steal the task and finish up before we invoke notify_all.
                        // Note: notify_once would also work but notify_all performs better if there is a lot of tasks at once.
                        _cv.notify_all();
                    }
                    else
                    {
                        // notify task that it should be wakeup and put into tasks queue.
                        _pausedTasks.emplace(i, DoNotPauseTask{});
                    }
                }
            };
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
                            {
                                using enum ETaskResumeState;

                                std::unique_lock queueLock{_tasksQueueMtx};
                                if (_cv.wait(queueLock, stopToken, [this] { return !_tasks.empty(); }) == false)
                                {
                                    // stop was requested
                                    return;
                                }

                                TaskT task{std::move(_tasks.front())};
                                _tasks.pop();

                                queueLock.unlock();

                                // resume the task
                                auto resumeState = std::invoke(task);

                                switch (resumeState)
                                {
                                case SUSPENDED: {
                                    queueLock.lock();
                                    _tasks.emplace(std::move(task));
                                    queueLock.unlock();

                                    _cv.notify_all();
                                    break;
                                }
                                case PAUSED: {

                                    auto id = task.id;

                                    std::unique_lock pauseLock{_pausedTasksMtx};

                                    if (auto it = _pausedTasks.find(id); it != _pausedTasks.end())
                                    {
                                        // need to wakeup the task.
                                        _pausedTasks.erase(it);
                                        pauseLock.unlock();

                                        queueLock.lock();
                                        _tasks.emplace(std::move(task));
                                        queueLock.unlock();
                                    }
                                    else
                                    {
                                        // put on pause the task.
                                        _pausedTasks.emplace(id, std::move(task));
                                    }

                                    break;
                                }
                                case STOPPED:
                                    [[fallthrough]];
                                case DONE: {
                                    // task is done
                                    _tasksCount.fetch_sub(1, std::memory_order_release);
                                    _tasksCount.notify_all();
                                    break;
                                }
                                default:
                                    break;
                                }
                            }
                        }
                    },
                    _stopSource.get_token());
            }
        }

        std::queue<TaskT>                       _tasks;
        std::unordered_map<cid_t, TaskVariantT> _pausedTasks;
        std::atomic<size_t>                     _tasksCount{0};

        std::vector<std::jthread> _workerThreads;
        std::stop_source          _stopSource;

        std::mutex _tasksQueueMtx;
        std::mutex _pausedTasksMtx;

        std::condition_variable_any _cv;

        std::atomic<cid_t> _coroutineIdCount{};
    };

    using Scheduler = CoroThreadPool<PackagedTask>;

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_SCHEDULER_HPP__
