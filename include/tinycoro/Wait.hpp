// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_WAIT_HPP
#define TINY_CORO_WAIT_HPP

#include <algorithm>
#include <tuple>
#include <vector>
#include <exception>
#include <type_traits>
#include <concepts>
#include <stop_token>

#include "Common.hpp"
#include "UnsafeFuture.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T, typename... Ts>
        concept AllSame = (std::same_as<T, Ts> && ...);

        template <typename T>
        concept Future = requires (T t) {
            { t.get() };
        };

        template <typename... Ts>
        concept AllFuture = (Future<Ts> && ...);

    } // namespace concepts

    namespace detail {
        using FutureVoid_t = std::optional<VoidType>;
    } // namespace detail

    template <template <typename> class FutureT, typename... Ts>
        requires (!concepts::AllSame<detail::FutureVoid_t, Ts...>)
    [[nodiscard]] auto GetAll(std::tuple<FutureT<Ts>&...>& futures)
    {
        std::exception_ptr exception;

        auto waiter = [&exception]<typename T>(FutureT<T>& f) {
            using opt_t = T;

            try
            {
                return f.get();
            }
            catch (...)
            {
                if (!exception)
                {
                    exception = std::current_exception();
                }
            }

            return opt_t{};
        };

        auto tupleResultOpt = std::apply([waiter]<typename... TypesT>(TypesT&... args) { return std::tuple{waiter(args)...}; }, futures);

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }

        if constexpr (sizeof...(Ts) == 1)
        {
            return std::move(std::get<0>(tupleResultOpt));
        }
        else
        {
            return tupleResultOpt;
        }
    }

    template <template <typename> class FutureT, typename... Ts>
        requires (!concepts::AllSame<detail::FutureVoid_t, Ts...>)
    [[nodiscard]] auto GetAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto tuple = std::apply([](auto&... elems) { return std::forward_as_tuple(elems...); }, futures);
        return GetAll(tuple);
    }

    template <template <typename> class FutureT, typename... Ts>
        requires concepts::AllSame<detail::FutureVoid_t, Ts...>
    void GetAll(std::tuple<FutureT<Ts>&...>& futures)
    {
        std::exception_ptr exception;

        auto futureGet = [&exception](auto& fut) {
            try
            {
                std::ignore = fut.get();
            }
            catch (...)
            {
                exception = std::current_exception();
            }
        };
        std::apply([futureGet](auto&... future) { ((futureGet(future)), ...); }, futures);

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }
    }

    template <template <typename> class FutureT, typename... Ts>
        requires concepts::AllSame<detail::FutureVoid_t, Ts...>
    void GetAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto tuple = std::apply([](auto&... elems) { return std::forward_as_tuple(elems...); }, futures);
        return GetAll(tuple);
    }

    template <template <typename> class FutureT, typename ReturnT>
        requires (!concepts::AllSame<detail::FutureVoid_t, ReturnT>)
    [[nodiscard]] auto GetAll(std::vector<FutureT<ReturnT>>& futures)
    {
        std::exception_ptr exception;

        std::vector<ReturnT> results;
        results.reserve(futures.size());

        for (auto& it : futures)
        {
            try
            {
                results.emplace_back(std::move(it.get()));
            }
            catch (...)
            {
                if (!exception)
                {
                    exception = std::current_exception();
                }
            }
        }

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }

        return results;
    }

    template <template <typename> class FutureT, typename ReturnT>
        requires concepts::AllSame<detail::FutureVoid_t, ReturnT>
    void GetAll(std::vector<FutureT<ReturnT>>& futures)
    {
        std::exception_ptr exception;

        for (auto& it : futures)
        {
            try
            {
                std::ignore = it.get();
            }
            catch (...)
            {
                if (!exception)
                {
                    exception = std::current_exception();
                }
            }
        }

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }
    }

    template <typename... FutureT>
        requires concepts::AllFuture<FutureT...>
    [[nodiscard]] auto GetAll(FutureT&&... futures)
    {
        auto tuple = std::forward_as_tuple(std::forward<FutureT>(futures)...);
        return GetAll(tuple);
    }

    template <typename SchedulerT, typename... Args>
        requires (sizeof...(Args) > 0) && concepts::IsScheduler<SchedulerT, Args...>
    [[nodiscard]] auto AllOf(SchedulerT& scheduler, Args&&... args)
    {
        auto future = scheduler.template Enqueue<tinycoro::unsafe::Promise>(std::forward<Args>(args)...);
        return GetAll(future);
    }

    template <typename SchedulerT, concepts::IsStopSource StopSourceT, concepts::IsCorouitneTask... CoroTasksT>
        requires (sizeof...(CoroTasksT) > 0)
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, StopSourceT source, CoroTasksT&&... tasks)
    {
        (tasks.SetStopSource(source), ...);

        auto futures = scheduler.template Enqueue<tinycoro::unsafe::Promise>(std::forward<CoroTasksT>(tasks)...);
        return GetAll(futures);
    }

    template <typename SchedulerT, concepts::IsStopSource StopSourceT, concepts::Iterable CoroContainerT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, StopSourceT source, CoroContainerT&& tasks)
    {
        std::ranges::for_each(tasks, [&source](auto& t) { t.SetStopSource(source); });

        auto futures = scheduler.template Enqueue<tinycoro::unsafe::Promise>(std::forward<CoroContainerT>(tasks));
        return GetAll(futures);
    }

    template <concepts::IsStopSource StopSourceT = std::stop_source, typename SchedulerT, concepts::IsCorouitneTask... CoroTasksT>
        requires (sizeof...(CoroTasksT) > 0) && concepts::IsScheduler<SchedulerT, CoroTasksT...>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroTasksT&&... tasks)
    {
        return AnyOf(scheduler, StopSourceT{}, std::forward<CoroTasksT>(tasks)...);
    }

    template <concepts::IsStopSource StopSourceT = std::stop_source, typename SchedulerT, concepts::Iterable CoroContainerT>
        requires concepts::IsScheduler<SchedulerT, CoroContainerT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroContainerT&& tasks)
    {
        return AnyOf(scheduler, StopSourceT{}, std::forward<CoroContainerT>(tasks));
    }

} // namespace tinycoro

#endif // TINY_CORO_WAIT_HPP