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
        using FutureVoid_t = FutureReturnT<void>::value_type;//  std::optional<VoidType>;

        template<template <typename> class FutureT, typename T>
        auto GetOne(FutureT<T>& future, std::exception_ptr& firstException)
        {
            using opt_t = T;

            try
            {
                return future.get();
            }
            catch (...)
            {
                if (!firstException)
                {
                    firstException = std::current_exception();
                }
            }

            return opt_t{};
        }


    } // namespace detail

    template <template <typename> class FutureT, typename... Ts>
        requires concepts::AllFuture<FutureT<Ts>...>
    [[nodiscard]] auto GetAll(FutureT<Ts>&... futures)
    {
        std::exception_ptr exception;
        [[maybe_unused]] std::tuple result{detail::GetOne(futures, exception)...};

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }

        if constexpr (!concepts::AllSame<detail::FutureVoid_t, Ts...>)
        {
            // in case we need to return something.
            if constexpr (sizeof...(Ts) == 1)
            {
                // we need this std::move here to support
                // only moveable types
                return std::move(std::get<0>(result));
            }
            else
            {
                return result;
            }
        }
    }


    template <template <typename> class FutureT, typename... Ts>
        requires concepts::AllFuture<FutureT<Ts>...>
        //requires (!concepts::AllSame<detail::FutureVoid_t, Ts...>)
    [[nodiscard]] auto GetAll(std::tuple<FutureT<Ts>...>& futures)
    {
        return std::apply([]<typename... ArgsT>(ArgsT&&... args) { return GetAll(std::forward<ArgsT>(args)...); }, futures);
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
            results.emplace_back(detail::GetOne(it, exception));
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
            std::ignore = detail::GetOne(it, exception);
        }

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }
    }

    template <typename SchedulerT, typename... Args>
        requires (sizeof...(Args) > 0) && concepts::IsScheduler<SchedulerT, Args...>
    [[nodiscard]] auto AllOf(SchedulerT& scheduler, Args&&... args)
    {
        auto future = scheduler.template Enqueue<tinycoro::unsafe::Promise>(std::forward<Args>(args)...);
        return GetAll(future);
    }

    template <typename SchedulerT, concepts::IsStopSource StopSourceT, typename... CoroTasksT>
        requires (sizeof...(CoroTasksT) > 0)
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, StopSourceT source, CoroTasksT&&... tasks)
    {
        detail::PropagateStopSource(detail::EStopSourcePolicy::STOP_SOURCE_USER, source, std::forward<CoroTasksT>(tasks)...);

        auto futures = scheduler.template Enqueue<tinycoro::unsafe::Promise>(std::forward<CoroTasksT>(tasks)...);
        return GetAll(futures);
    }

    template <concepts::IsStopSource StopSourceT = std::stop_source, typename SchedulerT, typename... CoroTasksT>
        requires (sizeof...(CoroTasksT) > 0) && concepts::IsScheduler<SchedulerT, CoroTasksT...>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroTasksT&&... tasks)
    {
        return AnyOf(scheduler, StopSourceT{}, std::forward<CoroTasksT>(tasks)...);
    }

} // namespace tinycoro

#endif // TINY_CORO_WAIT_HPP