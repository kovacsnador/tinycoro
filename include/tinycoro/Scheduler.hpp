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

#include "Future.hpp"
#include "Common.hpp"

#include "PackagedCoro.hpp"

using namespace std::chrono_literals;

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept Iterable = requires (T) {
            typename std::decay_t<T>::iterator;
            typename std::decay_t<T>::value_type;
        };

        template <typename T>
        concept NonIterable = !Iterable<T>;

    } // namespace concepts

    template <std::move_constructible TaskT, template <typename> class FutureStateT>
        requires requires (TaskT t) {
            { std::invoke(t) } -> std::same_as<ECoroResumeState>;
            { t.Pause(typename TaskT::PauseCallbackType{}) };
        } && concepts::FutureState<FutureStateT<void>>
    class CoroThreadPool
    {
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
        }

        // Disable copy and move
        CoroThreadPool(const CoroThreadPool&) = delete;
        CoroThreadPool(CoroThreadPool&&)      = delete;

        template <typename CoroT>
            requires (!std::is_reference_v<CoroT>) && requires (CoroT) { typename CoroT::promise_type::value_type; }
        [[nodiscard]] auto Enqueue(CoroT&& coro)
        {
            FutureStateT<typename CoroT::promise_type::value_type> futureState;

            auto future = futureState.get_future();

            _tasksCount.fetch_add(1, std::memory_order_acquire);

            {
                std::scoped_lock lock{_mtx};
                _tasks.emplace(std::move(coro), std::move(futureState));
            }

            _cv.notify_all();

            return future;
        }

        template <concepts::NonIterable... CoroTs>
        [[nodiscard]] auto EnqueueTasks(CoroTs&&... tasks)
        {
            return std::make_tuple(Enqueue(std::forward<CoroTs>(tasks))...);
        }

        template <concepts::Iterable ContainerT>
        [[nodiscard]] auto EnqueueTasks(ContainerT&& tasks)
        {
            using FutureStateType = FutureStateT<typename std::decay_t<ContainerT>::value_type::promise_type::value_type>;

            std::vector<decltype(std::declval<FutureStateType>().get_future())> futures;
            futures.reserve(tasks.size());

            for (auto&& task : tasks)
            {
                if constexpr (std::is_rvalue_reference_v<decltype(tasks)>)
                {
                    futures.emplace_back(Enqueue(std::move(task)));
                }
                else
                {
                    futures.emplace_back(Enqueue(task.task_view()));
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
                                using enum ECoroResumeState;

                                std::unique_lock lock{_mtx};
                                if (_cv.wait(lock, stopToken, [this] { return !_tasks.empty(); }) == false)
                                {
                                    // stop was requested
                                    return;
                                }

                                TaskT task{std::move(_tasks.front())};
                                _tasks.pop();

                                lock.unlock();

                                // resume the task
                                auto resumeState = std::invoke(task);

                                if (resumeState == SUSPENDED)
                                {
                                    lock.lock();
                                    _tasks.emplace(std::move(task));
                                    lock.unlock();

                                    _cv.notify_all();
                                }
                                else if (resumeState == PAUSED)
                                {
                                    static size_t id = 0;

                                    lock.lock();

                                    task.Pause([this, i = id] {
                                        {
                                            std::scoped_lock lock{_mtx};
                                            if (auto it = _pausedTasks.find(i); it != _pausedTasks.end())
                                            {
                                                _tasks.emplace(std::move(it->second));
                                            }
                                            _pausedTasks.erase(i);
                                        }

                                        _cv.notify_all();
                                    });

                                    _pausedTasks.emplace(id++, std::move(task));

                                    lock.unlock();
                                }
                                else if (resumeState == DONE)
                                {
                                    // task is done
                                    _tasksCount.fetch_sub(1, std::memory_order_release);
                                    _tasksCount.notify_all();
                                }
                            }
                        }
                    },
                    _stopSource.get_token());
            }
        }

        std::queue<TaskT>                 _tasks;
        std::unordered_map<size_t, TaskT> _pausedTasks;
        std::atomic<size_t>               _tasksCount{0};

        std::vector<std::jthread> _workerThreads;
        std::stop_source          _stopSource;

        std::mutex                  _mtx;
        std::condition_variable_any _cv;
    };

    using CoroScheduler = CoroThreadPool<PackagedCoro<>, std::promise>;
    // using CoroScheduler = CoroThreadPool<PackedSchedulableTask<>, FutureState>;

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_SCHEDULER_HPP__
