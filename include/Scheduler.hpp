#ifndef __CORO_SCHEDULER_HPP__
#define __CORO_SCHEDULER_HPP__

#include <thread>
#include <future>
#include <functional>
#include <vector>
#include <variant>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <concepts>
#include <assert.h>
#include <ranges>
#include <memory>
#include <algorithm>

#include "Future.hpp"
#include "Common.hpp"
#include "Storage.hpp"

using namespace std::chrono_literals;

namespace concepts
{
	template<typename T>
	concept FutureState = (requires(T f) { { f.set_value() }; } || requires(T f) { { f.set_value(f.get_future().get()) }; } && requires(T f) { f.set_exception(std::exception_ptr{}); });

	template<typename CoroT>
	concept Resumable = requires(CoroT c) { { c.resume() } -> std::same_as<ECoroResumeState>; };
}

template<std::move_constructible TaskT, template<typename> class FutureStateT>
	requires requires(TaskT t) {
		{std::invoke(t)} -> std::same_as<ECoroResumeState>;
		{t.Pause(typename TaskT::PauseCallbackType{})};
	} && 
	concepts::FutureState<FutureStateT<void>>
class CoroThreadPool
{
public:
	CoroThreadPool(size_t workerThreadCount)
	{
		_AddWorkers(workerThreadCount);
	}

	~CoroThreadPool()
	{
		_stopSource.request_stop();
		std::ranges::for_each(_workerThreads, [](auto& it) { if (it.joinable()) { it.join(); } });
	}

	// Disable copy and move
	CoroThreadPool(const CoroThreadPool&) = delete;
	CoroThreadPool(CoroThreadPool&&) = delete;

	template<typename T>
		requires (!std::is_reference_v<T>) && requires(T) { typename T::promise_type::value_type; }
	auto Enqueue(T&& task)
	{
		FutureStateT<typename T::promise_type::value_type> futureState;
		auto future = futureState.get_future();
			
		_tasksCount.fetch_add(1, std::memory_order_acquire);

		{
			std::scoped_lock lock{ _mtx };
			_tasks.emplace(std::move(task), std::move(futureState));
		}

		_cv.notify_all();

		return future;
	}

	template<typename... Tasks>
	auto EnqueueTasks(Tasks&&... tasks)
	{
		return std::make_tuple(Enqueue(std::forward<Tasks>(tasks))...);
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
			_workerThreads.emplace_back([this](std::stop_token stopToken) {
				while (stopToken.stop_requested() == false)
				{
					{
						using enum ECoroResumeState;

						std::unique_lock lock{ _mtx };
						if (_cv.wait(lock, stopToken, [this] {return !_tasks.empty(); }) == false)
						{
							// stop was requested
							return;
						}

						TaskT task{ std::move(_tasks.front()) };
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
									std::scoped_lock lock{ _mtx };
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
						else if(resumeState == DONE)
						{
							// task is done
							_tasksCount.fetch_sub(1, std::memory_order_release);
							_tasksCount.notify_all();
						}
					}

					// TODO remove (only for testing)
					std::this_thread::sleep_for(100ms);

				}
			}, _stopSource.get_token());
		}
	}

	std::queue<TaskT>				  _tasks;
	std::unordered_map<size_t, TaskT> _pausedTasks;
	std::atomic<size_t>				  _tasksCount{0};

	std::vector<std::jthread> _workerThreads;
	std::stop_source		  _stopSource;

	std::mutex					_mtx;
	std::condition_variable_any _cv;
};

template<std::unsigned_integral auto BUFFER_SIZE = 64u>
struct PackedSchedulableTask
{
	using PauseCallbackType = std::function<void()>;

private:
	class ISchedulableBridged
	{
	public:
		virtual ~ISchedulableBridged() = default;
		virtual ECoroResumeState resume() = 0;
		virtual void pause(PauseCallbackType) = 0;
	};

	template<concepts::Resumable CoroT, concepts::FutureState FutureStateT>
	class SchedulableBridgeImpl : public ISchedulableBridged
	{
	public:
		SchedulableBridgeImpl(CoroT&& coro, FutureStateT&& futureState)
			: _coro{ std::move(coro) }
			, _futureState{ std::move(futureState) }
		{
		}

		SchedulableBridgeImpl(SchedulableBridgeImpl&&) = delete;

		~SchedulableBridgeImpl()
		{
			if(_exceptionSet == false)
			{
				if constexpr (requires{ _coro.hdl.promise().ReturnValue(); })
				{
					_futureState.set_value(std::move(_coro.hdl.promise().ReturnValue()));
				}
				else
				{
					_futureState.set_value();
				}
			}
		}

		ECoroResumeState resume() override
		{
			ECoroResumeState resumeState{ ECoroResumeState::DONE };

			try
			{
				resumeState = _coro.resume();
			}
			catch (...)
			{
				_futureState.set_exception(std::current_exception());
				_exceptionSet = true;
			}

			return resumeState;
		}

		void pause(PauseCallbackType func) override
		{
			_coro.hdl.promise().pauseResume = std::move(func);
		}

	private:
		bool			_exceptionSet{false};
		CoroT			_coro;
		FutureStateT	_futureState;
	};

	using StaticStorageType = Storage<ISchedulableBridged, BUFFER_SIZE>;
	using DynamicStorageType = std::unique_ptr<ISchedulableBridged>;

public:

	template<concepts::Resumable CoroT, concepts::FutureState FutureStateT>
		requires (!std::is_reference_v<CoroT>) && (!std::same_as<std::decay_t<CoroT>, PackedSchedulableTask>)
	PackedSchedulableTask(CoroT&& coro, FutureStateT futureState)
		//: _bridge{std::make_unique<SchedulableBridgeImpl<CoroT, FutureStateT>>(std::move(coro), std::move(futureState))}
	{
		using BridgeType = SchedulableBridgeImpl<CoroT, FutureStateT>;

		SyncOut() << "sizeof(BridgeType): " << sizeof(BridgeType) << '\n';

		if constexpr (sizeof(BridgeType) <= BUFFER_SIZE)
		{
			_bridge = Storage<ISchedulableBridged, BUFFER_SIZE>{std::type_identity<BridgeType>{}, std::move(coro), std::move(futureState)};
		}
		else
		{
			_bridge = std::make_unique<BridgeType>(std::move(coro), std::move(futureState));
		}
	}

	ECoroResumeState operator()()
	{
		return std::visit([](auto& bridge){ return bridge->resume(); }, _bridge);
	}

	void Pause(std::invocable auto pauseCallback)
	{
		std::visit([&pauseCallback](auto& bridge){ bridge->pause(std::move(pauseCallback)); }, _bridge);
	}

private:

	//DynamicStorageType _bridge;

	std::variant<StaticStorageType, DynamicStorageType> _bridge;
};

//using CoroScheduler = CoroThreadPool<PackedSchedulableTask<48u>, std::promise>;
using CoroScheduler = CoroThreadPool<PackedSchedulableTask<>, FutureState>;

#endif // !__CORO_SCHEDULER_HPP__
