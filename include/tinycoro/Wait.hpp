#ifndef __TINY_CORO_WAIT_HPP__
#define __TINY_CORO_WAIT_HPP__

#include <algorithm>
#include <tuple>
#include <vector>
#include <exception>
#include <type_traits>
#include <concepts>
#include <stop_token>

#include "Common.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T, typename... Ts>
        concept AllSame = (std::same_as<T, Ts> && ...);

        template <typename T>
        concept Future = requires (T t) {
            { t.get() };
            { t.valid() } -> std::same_as<bool>;
        };

        template <typename... Ts>
        concept AllFuture = (Future<Ts> && ...);

    } // namespace concepts

    struct VoidType
    {
    };

    template <template <typename> class FutureT, typename... Ts>
        requires (!concepts::AllSame<void, Ts...>)
    [[nodiscard]] auto GetAll(std::tuple<FutureT<Ts>&...>& futures)
    {
        std::exception_ptr exception;

        auto waiter = [&exception]<typename T>(FutureT<T>& f) {
            if constexpr (std::same_as<void, T>)
            {
                using opt_t = std::optional<VoidType>;

                try
                {
                    f.get();
                    return opt_t{VoidType{}};
                }
                catch (...)
                {
                    if (!exception)
                    {
                        exception = std::current_exception();
                    }
                }

                return opt_t{};
            }
            else
            {
                using opt_t = std::optional<T>;

                try
                {
                    return opt_t(std::move(f.get()));
                }
                catch (...)
                {
                    if (!exception)
                    {
                        exception = std::current_exception();
                    }
                }

                return opt_t{};
            }
        };

        auto tupleResultOpt = std::apply([waiter]<typename... TypesT>(TypesT&... args) { return std::make_tuple(waiter(args)...); }, futures);

        if (exception)
        {
            // rethrows the first exception
            std::rethrow_exception(exception);
        }

        auto optConverter = []<typename T>(std::optional<T>& o) { return std::move(o.value()); };

        auto resultTuple
            = std::apply([optConverter]<typename... TypesT>(TypesT&... args) { return std::make_tuple(optConverter(args)...); }, tupleResultOpt);

        if constexpr (sizeof...(Ts) == 1)
        {
            return std::move(std::get<0>(resultTuple));
        }
        else
        {
            return resultTuple;
        }
    }

    template <template <typename> class FutureT, typename... Ts>
        requires (!concepts::AllSame<void, Ts...>)
    [[nodiscard]] auto GetAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto tuple = std::apply([](auto&... elems) { return std::forward_as_tuple(elems...); }, futures);
        return GetAll(tuple);
    }

    template <template <typename> class FutureT, typename... Ts>
        requires concepts::AllSame<void, Ts...>
    void GetAll(std::tuple<FutureT<Ts>&...>& futures)
    {
        std::exception_ptr exception;

        auto futureGet = [&exception](auto& fut) {
            try
            {
                fut.get();
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
        requires concepts::AllSame<void, Ts...>
    void GetAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto tuple = std::apply([](auto&... elems) { return std::forward_as_tuple(elems...); }, futures);
        return GetAll(tuple);
    }

    template <template <typename> class FutureT, typename ReturnT>
        requires (!concepts::AllSame<void, ReturnT>)
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
        requires concepts::AllSame<void, ReturnT>
    void GetAll(std::vector<FutureT<ReturnT>>& futures)
    {
        std::exception_ptr exception;

        for (auto& it : futures)
        {
            try
            {
                it.get();
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

    template <typename SchedulerT, typename... Args>
        requires requires(SchedulerT s, Args... a) { { s.Enqueue(std::forward<Args>(a)...)}; }
    [[nodiscard]] auto GetAll(SchedulerT& scheduler, Args&&... args)
    {
        auto future = scheduler.Enqueue(std::forward<Args>(args)...);
        return GetAll(future);
    }

    template <typename... FutureT>
        requires concepts::AllFuture<FutureT...>
    [[nodiscard]] auto GetAll(FutureT&&... futures)
    {
        auto tuple = std::forward_as_tuple(std::forward<FutureT>(futures)...); // std::make_tuple(std::forward<FutureT<Ts>>(futures)...);
        return GetAll(tuple);
    }

    template <typename SchedulerT, typename StopSourceT, typename... CoroTasksT>
    [[nodiscard]] auto AnyOfWithStopSource(SchedulerT& scheduler, StopSourceT source, CoroTasksT&&... tasks)
    {
        (tasks.SetStopSource(source), ...);

        auto futures = scheduler.Enqueue(std::forward<CoroTasksT>(tasks)...);
        return GetAll(futures);
    }

    template <typename SchedulerT, typename StopSourceT, typename CoroContainerT>
    [[nodiscard]] auto AnyOfWithStopSource(SchedulerT& scheduler, StopSourceT source, CoroContainerT&& tasks)
    {
        std::ranges::for_each(tasks, [&source](auto& t) { t.SetStopSource(source); });

        auto futures = scheduler.Enqueue(std::forward<CoroContainerT>(tasks));
        return GetAll(futures);
    }

    template <typename StopSourceT = std::stop_source, typename SchedulerT, typename... CoroTasksT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroTasksT&&... tasks)
    {
        return AnyOfWithStopSource(scheduler, StopSourceT{}, std::forward<CoroTasksT>(tasks)...);
    }

    template <typename StopSourceT = std::stop_source, typename SchedulerT, typename CoroContainerT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroContainerT&& tasks)
    {
        return AnyOfWithStopSource(scheduler, StopSourceT{}, std::forward<CoroContainerT>(tasks));
    }

} // namespace tinycoro

#endif //!__TINY_CORO_WAIT_HPP__