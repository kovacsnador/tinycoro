#ifndef __TINY_CORO_RUN_INLINE_HPP__
#define __TINY_CORO_RUN_INLINE_HPP__

#include <type_traits>
#include <atomic>
#include <concepts>
#include <barrier>
#include <optional>

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

        template <typename TaskT, typename EventT>
        void SetPauseResumerCallback(TaskT& task, EventT& event)
        {
            auto pauseResumerCallback = [&task, &event] {
                auto pauseHandler = task.GetPauseHandler();

                // unpause the task with explicitly calling resume on pauseHandler
                pauseHandler->Resume();

                // sets the event, that means theres is a task 
                // which is ready for resumption.
                event.Set();
            };
            task.SetPauseHandler(pauseResumerCallback);
        }

        template <concepts::LocalRunable TaskT>
        auto TryResume(TaskT& task)
        {
            if (task.IsPaused() == false && task.IsDone() == false)
            {
                // resume the corouitne if no pause and not done
                // if the task is in a cancellable suspend, and the task a stop is requested
                // the function simply returns and does nothing
                task.Resume();
            }

            // after resumption getting the state
            return task.ResumeState();
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

        // runs all the tasks inline on the current thread.
        template <typename TupleT>
        class InlineScheduler
        {
        public:
            // constructor
            InlineScheduler(TupleT tuple)
            : _tasks{std::move(tuple)}
            {
            }

            // disable copy and move
            InlineScheduler(InlineScheduler&&) = delete;

            // run all the tasks sequntially
            auto Run()
            {
                helper::AutoResetEvent event;

                // set pause handler for all tasks
                std::apply([&event](auto&&... tasks) { (SetPauseResumerCallback(tasks, event), ...); }, _tasks);

                for (;;)
                {
                    // resume acitive coroutines
                    auto collectedStatus = std::apply([](auto&&... tasks) { return std::tuple{TryResume(tasks)...}; }, _tasks);

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

                auto resultConverter = [](auto& task) {
                    if constexpr (requires { { task.await_resume() } -> std::same_as<void>; })
                    {
                        std::optional<VoidType> result{};
                        if(task.IsDone())
                        {
                            result = VoidType{};
                        }
                        return result;
                    }
                    else
                    {
                        using return_t = std::decay_t<decltype(task)>::value_type;

                        std::optional<return_t> result{};
                        if(task.IsDone())
                        {
                            result = std::move(task.await_resume());
                        }
                        return result;
                    }
                };

                // collecting and return all the return values.
                auto resultsTuple = std::apply([resultConverter]<typename... TypesT>(TypesT&... args) { return std::tuple{resultConverter(args)...}; }, _tasks);

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

            TupleT _tasks;
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

    namespace detail {

        template <concepts::Iterable ContainerT>
        void RunInlineImplContainer(ContainerT&& container)
        {
            helper::AutoResetEvent event;

            for (auto& it : container)
            {
                detail::SetPauseResumerCallback(it, event);
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
                    resultStates.push_back(detail::TryResume(it));
                }

                // first check:
                // if every task is done/stopped state.
                if (commonChecker(resultStates, IsDoneChecker))
                {
                    // tasks are done.
                    return;
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
        }

    } // namespace detail

    template <concepts::Iterable ContainerT>
        requires (!std::same_as<typename std::decay_t<ContainerT>::value_type::value_type, void>)
    [[nodiscard]] auto RunInline(ContainerT&& container)
    {
        // Runs all the tasks sequentialy
        detail::RunInlineImplContainer(container);

        // container to hold the result values.
        std::vector<std::optional<typename std::decay_t<ContainerT>::value_type::value_type>> results;
        results.reserve(std::size(container));

        // collecting all the results from the tasks through await_resume function.
        for (auto& it : container)
        {
            if(it.IsDone())
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

    template <concepts::Iterable ContainerT>
        requires std::same_as<typename std::decay_t<ContainerT>::value_type::value_type, void>
    void RunInline(ContainerT&& container)
    {
        // Runs all the tasks sequentialy
        detail::RunInlineImplContainer(container);
    }

} // namespace tinycoro

#endif //!__TINY_CORO_RUN_INLINE_HPP__