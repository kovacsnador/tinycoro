#ifndef __TINY_CORO_RUN_INLINE_HPP__
#define __TINY_CORO_RUN_INLINE_HPP__

#include <type_traits>
#include <atomic>
#include <concepts>
#include <barrier>
#include <optional>
#include <stop_token>

#include "Common.hpp"
#include "PauseHandler.hpp"

namespace tinycoro {
    namespace concepts {

        template <typename T>
        concept LocalRunable = requires (T t) {
            { t.await_resume() };
            { t.GetPauseHandler() };
            { t.SetPauseHandler([] {}) };
            { t.Resume() } -> std::same_as<void>;
            { t.ResumeState() } -> std::same_as<ETaskResumeState>;
        };

        template <typename ValueT, typename... TaskT>
        concept SameAsValueType = (std::same_as<ValueT, typename std::decay_t<TaskT>::value_type> && ...);
    } // namespace concepts

    namespace detail {

        template <typename TaskT, typename EventT, typename StopTokenT>
        void SetPauseResumerCallback(TaskT& task, EventT& event, StopTokenT stopToken)
        {
            auto pauseResumerCallback = [&task, &event, stopToken] {
                auto pauseHandler = task.GetPauseHandler();

                // checking if the task is cancelled
                auto isTaskCancelled = [&stopToken, &pauseHandler]() {
                    if(stopToken.stop_requested())
                    {
                        return pauseHandler->IsCancellable();
                    }
                    return false;
                };

                if (isTaskCancelled() == false)
                {
                    // it the task is not cancelled,
                    // reset the pause handler flags with explicitly
                    // calling resume on pauseHandler
                    pauseHandler->Resume();
                }

                // sets the event, that means theres is a task
                // which is ready for resumption.
                event.Set();
            };

            // setup the resumer callback
            task.SetPauseHandler(pauseResumerCallback);
        }

        // check if the task is finished
        // both if "done" or "stopped" means the task is done.
        constexpr bool IsDoneChecker(ETaskResumeState state)
        {
            return state == ETaskResumeState::DONE || state == ETaskResumeState::STOPPED;
        };

        // Check if one of the tasks is runnable,
        // or we should wait for at least one task to be notified for resumption.
        constexpr bool NeedWaitChecker(ETaskResumeState state)
        {
            return IsDoneChecker(state) || state == ETaskResumeState::PAUSED;
        };

        template <concepts::LocalRunable TaskT>
        auto TryResume(TaskT& task, std::exception_ptr& exception, auto& stopSource)
        {
            if (task.IsPaused() == false && task.IsDone() == false)
            {
                try
                {
                    // resume the corouitne if not in pause and not in done state.
                    // if the task is in a cancellable suspend, and a stop is requested
                    // the function simply returns and does nothing
                    task.Resume();
                }
                catch (...)
                {
                    if (!exception)
                    {
                        // save the first exception.
                        exception = std::current_exception();
                    }
                }
            }

            // after resumption getting the state
            auto state = task.ResumeState();

            if (stopSource.stop_possible() && stopSource.stop_requested() == false && IsDoneChecker(state))
            {
                // if stop is possible we are
                // in an AnyOf context.
                // So we check if somebody is already done
                // and requesting the stop
                stopSource.request_stop();
            }

            return state;
        }

        // runs all the tasks inline on the current thread.
        template <typename TupleT, typename StopSourceT = std::stop_source>
        class InlineScheduler
        {
        public:
            // constructor
            InlineScheduler(TupleT tuple)
            : _tasks{std::move(tuple)}
            {
            }

            // constructor
            InlineScheduler(StopSourceT stopSource, TupleT tuple)
            : _tasks{std::move(tuple)}
            , _stopSource{stopSource}
            {
            }

            // disable copy and move
            InlineScheduler(InlineScheduler&&) = delete;

            // run all the tasks sequntially
            auto Run()
            {
                std::exception_ptr     exception{};
                helper::AutoResetEvent event{};

                auto stopToken = _stopSource.get_token();

                std::stop_callback stopCallback{stopToken, [&event] {
                                                    // trigger the event
                                                    // if stop was requested
                                                    event.Set();
                                                }};

                if (_stopSource.stop_possible())
                {
                    // if we have a common stopsource (AnyOf context),
                    // we are going to set it for the tasks
                    std::apply([this](auto&&... tasks) { (tasks.SetStopSource(_stopSource), ...); }, _tasks);
                }

                // set pause handler for all tasks
                std::apply([&stopToken, &event](auto&&... tasks) { (SetPauseResumerCallback(tasks, event, stopToken), ...); }, _tasks);

                for (;;)
                {
                    // resume acitive coroutines
                    auto collectedStatus
                        = std::apply([&exception, this](auto&&... tasks) { return std::tuple{TryResume(tasks, exception, _stopSource)...}; }, _tasks);

                    // 1. check:
                    // if every task is done or in a stopped state
                    if (HasCommonState(collectedStatus, IsDoneChecker))
                    {
                        // all done
                        break;
                    }

                    // 2. check:
                    // if every task wether done/stopped
                    // and at least one in paused state
                    if (HasCommonState(collectedStatus, NeedWaitChecker))
                    {
                        // most tasks are finished but we have at least
                        // one paused task, so we need to wait on this thread...
                        // Waits until somebody get's notified to resume
                        event.Wait();
                    }
                }

                if (exception)
                {
                    // if we had an exception
                    std::rethrow_exception(exception);
                }

                auto resultConverter = []<typename T>(T& task) {
                    if constexpr (requires {
                                      { task.await_resume() } -> std::same_as<void>;
                                  })
                    {
                        TaskResult_t<VoidType> result{};
                        if (task.IsDone())
                        {
                            result = VoidType{};
                        }
                        return result;
                    }
                    else
                    {
                        using return_t = typename T::value_type;

                        TaskResult_t<return_t> result{};
                        if (task.IsDone())
                        {
                            result = std::move(task.await_resume());
                        }
                        return result;
                    }
                };

                // collecting and return all the return values.
                auto resultsTuple
                    = std::apply([resultConverter]<typename... TypesT>(TypesT&... args) { return std::tuple{resultConverter(args)...}; }, _tasks);

                if constexpr (std::tuple_size_v<decltype(resultsTuple)> == 1)
                {
                    // Test if we need this std::move here....
                    return std::move(std::get<0>(resultsTuple));
                }
                else
                {
                    return resultsTuple;
                }
            }

        private:
            template <typename CmpFunctionT, typename... T>
            bool HasCommonState(const std::tuple<T...>& tupleStates, CmpFunctionT cmp)
            {
                auto boolConverter = [&cmp](ETaskResumeState s) { return cmp(s); };

                auto boolTuple = std::apply([&boolConverter](auto... states) { return std::tuple{boolConverter(states)...}; }, tupleStates);

                return std::apply(
                    [](auto... states) {
                        bool result{true};
                        (result &= ... &= states);
                        return result;
                    },
                    boolTuple);
            }

            TupleT      _tasks;
            StopSourceT _stopSource{std::nostopstate};
        };
    } // namespace detail

    // runs a simple task/tasks inline on current thread.
    // Ideal if you want to run a task in a non coroutine environment,
    // and you anyway want to wait for the result. (No need for a scheduler)
    template <typename... TaskT>
        requires (sizeof...(TaskT) > 0) && (!concepts::SameAsValueType<void, TaskT...>)
    [[nodiscard]] auto RunInline(TaskT&&... tasks)
    {
        detail::InlineScheduler inlineScheduler{std::forward_as_tuple(tasks...)};
        return inlineScheduler.Run();
    }

    template <typename... TaskT>
        requires (sizeof...(TaskT) > 0) && concepts::SameAsValueType<void, TaskT...>
    void RunInline(TaskT&&... tasks)
    {
        detail::InlineScheduler inlineScheduler{std::forward_as_tuple(tasks...)};
        std::ignore = inlineScheduler.Run();
    }

    template <typename StopSourceT = std::stop_source, typename... TaskT>
        requires (sizeof...(TaskT) > 0) && (!concepts::SameAsValueType<void, TaskT...>)
    [[nodiscard]] auto AnyOfWithStopSourceInline(StopSourceT stopSource, TaskT&&... tasks)
    {
        detail::InlineScheduler inlineScheduler{stopSource, std::forward_as_tuple(tasks...)};
        return inlineScheduler.Run();
    }

    template <typename StopSourceT = std::stop_source, typename... TaskT>
        requires (sizeof...(TaskT) > 0) && concepts::SameAsValueType<void, TaskT...>
    void AnyOfWithStopSourceInline(StopSourceT stopSource, TaskT&&... tasks)
    {
        detail::InlineScheduler inlineScheduler{stopSource, std::forward_as_tuple(tasks...)};
        std::ignore = inlineScheduler.Run();
    }

    template <typename StopSourceT = std::stop_source, concepts::NonIterable... TaskT>
        requires (sizeof...(TaskT) > 0)
    [[nodiscard]] auto AnyOfInline(TaskT&&... tasks)
    {
        StopSourceT stopSource;
        return AnyOfWithStopSourceInline(stopSource, std::forward<TaskT>(tasks)...);
    }

    namespace detail {

        template <typename ContainerT>
        using TaskReturnT = typename std::decay_t<ContainerT>::value_type::value_type;

        template <concepts::Iterable ContainerT, typename StopSourceT>
        void RunInlineImplContainer(ContainerT&& container, StopSourceT stopSource)
        {
            std::exception_ptr     exception{};
            helper::AutoResetEvent event{};

            auto stopToken = stopSource.get_token();

            std::stop_callback stopCallback{stopToken, [&event] {
                                                // trigger the event
                                                // if stop was requested
                                                event.Set();
                                            }};

            if (stopSource.stop_possible())
            {
                // if we have a common stopsource (AnyOf context),
                // we are going to set it for the tasks
                for (auto& it : container)
                {
                    it.SetStopSource(stopSource);
                }
            }

            for (auto& it : container)
            {
                detail::SetPauseResumerCallback(it, event, stopToken);
            }

            std::vector<ETaskResumeState> resultStates;
            resultStates.reserve(std::size(container));

            // Checks if there are any common states in results after asking for ResultState in tasks.
            auto commonChecker = [](const std::vector<ETaskResumeState>& resultStates, auto cmpFunction) {
                bool result{true};
                for (const auto& it : resultStates)
                {
                    result &= cmpFunction(it);
                }
                return result;
            };

            for (;;)
            {
                // Resume the tasks if they are not done yet.
                for (auto& it : container)
                {
                    resultStates.push_back(detail::TryResume(it, exception, stopSource));
                }

                // first check:
                // if every task is done/stopped state.
                if (commonChecker(resultStates, IsDoneChecker))
                {
                    // tasks are done.
                    break;
                }

                // second check:
                // if every task wether done/stopped or in pause state
                if (commonChecker(resultStates, NeedWaitChecker))
                {
                    // all in pause state or done, so we need to wait on this thread...
                    // Waits until somebody get's notified to resume
                    event.Wait();
                }

                // clear the vector for the next batch of results
                resultStates.clear();
            }

            if (exception)
            {
                // rethrow the first exception
                std::rethrow_exception(exception);
            }
        }

        template <typename ContainerT>
        auto CollectResults(ContainerT&& container)
        {
            // container to hold the result values.
            std::vector<TaskResult_t<TaskReturnT<ContainerT>>> results;
            results.reserve(std::size(container));

            // collecting all the results from the tasks through await_resume function.
            for (auto& it : container)
            {
                if (it.IsDone())
                {
                    // if done we return the
                    // await_resume return value.
                    results.emplace_back(it.await_resume());
                }
                else
                {
                    // if it was cancelled add
                    // nullopt as the result.
                    results.emplace_back(std::nullopt);
                }
            }

            return results;
        }

    } // namespace detail

    template <concepts::Iterable ContainerT>
        requires (!std::same_as<detail::TaskReturnT<ContainerT>, void>)
    [[nodiscard]] auto RunInline(ContainerT&& container)
    {
        // Runs all the tasks sequentialy
        detail::RunInlineImplContainer(container, std::stop_source{std::nostopstate});
        return detail::CollectResults(container);
    }

    template <concepts::Iterable ContainerT>
        requires std::same_as<detail::TaskReturnT<ContainerT>, void>
    void RunInline(ContainerT&& container)
    {
        // Runs all the tasks sequentialy
        detail::RunInlineImplContainer(container, std::stop_source{std::nostopstate});
    }

    template <concepts::Iterable ContainerT, typename StopSourceT>
        requires (!std::same_as<detail::TaskReturnT<ContainerT>, void>)
    [[nodiscard]] auto AnyOfWithStopSourceInline(StopSourceT stopSource, ContainerT&& container)
    {
        // Runs all the tasks sequentialy
        detail::RunInlineImplContainer(container, stopSource);
        return detail::CollectResults(container);
    }

    template <concepts::Iterable ContainerT, typename StopSourceT>
        requires std::same_as<detail::TaskReturnT<ContainerT>, void>
    void AnyOfWithStopSourceInline(StopSourceT stopSource, ContainerT&& container)
    {
        // Runs all the tasks sequentialy
        detail::RunInlineImplContainer(container, stopSource);
    }

    template <concepts::Iterable ContainerT, typename StopSourceT = std::stop_source>
    [[nodiscard]] auto AnyOfInline(ContainerT&& container)
    {
        StopSourceT stopSource{};
        return AnyOfWithStopSourceInline(stopSource, std::forward<ContainerT>(container));
    }

} // namespace tinycoro

#endif //!__TINY_CORO_RUN_INLINE_HPP__