#ifndef __TINY_CORO_WAIT_HPP__
#define __TINY_CORO_WAIT_HPP__

#include <algorithm>
#include <tuple>
#include <vector>
#include <exception>
#include <type_traits>
#include <concepts>
#include <stop_token>
#include <memory_resource>
#include <array>

#include "Common.hpp"
#include "PackagedTask.hpp"
#include "Task.hpp"

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

    namespace detail {
        using FutureVoid_t = std::optional<VoidType>;

        struct Placeholder
        {
            int32_t p;
        };

        using averageTask_t = detail::SchedulableBridgeImpl<tinycoro::Task<int32_t>, std::promise<int32_t>, std::pmr::polymorphic_allocator<>>;

        // This is the task size.
        // should be the same for everyone
        constexpr size_t c_taskSize = sizeof(averageTask_t);

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
                // Check if this move here is needed
                return std::move(f.get());
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

        // std::vector<typename ReturnT::value_type> results;
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

    template <typename SchedulerT, concepts::IsCorouitneTask... Args>
        requires requires (SchedulerT s, Args... a) {
            { s.Enqueue(std::forward<Args>(a)...) };
        }
    [[nodiscard]] auto GetAll(SchedulerT& scheduler, Args&&... args)
    {
        /*constexpr size_t size = sizeof(detail::averageTask_t);
        constexpr size_t count = sizeof...(args);

        alignas(detail::averageTask_t) std::array<std::byte, sizeof(detail::averageTask_t) * (sizeof...(args) + 1)> buffer;
        std::pmr::monotonic_buffer_resource mbf{buffer.data(), buffer.size()};
        std::pmr::polymorphic_allocator<> allocator{&mbf};*/

        auto future = scheduler.Enqueue(/*&allocator,*/ std::forward<Args>(args)...);
        return GetAll(future);
    }

    template <typename SchedulerT, concepts::Iterable CoroContainerT>
    [[nodiscard]] auto GetAll(SchedulerT& scheduler, CoroContainerT&& container)
    {
        /*alignas(averageTask_t) std::array<std::byte, sizeof(averageTask_t) * sizeof...(args)> buffer;
        std::pmr::monotonic_buffer_resource mbf{buffer.data(), buffer.size()};
        std::pmr::polymorphic_allocator<> allocator{&mbf};*/

        auto future = scheduler.Enqueue(std::forward<CoroContainerT>(container));
        return GetAll(future);
    }

    template <typename SchedulerT, typename StopSourceT, concepts::NonIterable... CoroTasksT>
    [[nodiscard]] auto AnyOfWithStopSource(SchedulerT& scheduler, StopSourceT source, CoroTasksT&&... tasks)
    {
        (tasks.SetStopSource(source), ...);

        auto futures = scheduler.Enqueue(std::forward<CoroTasksT>(tasks)...);
        return GetAll(futures);
    }

    template <typename SchedulerT, typename StopSourceT, concepts::Iterable CoroContainerT>
    [[nodiscard]] auto AnyOfWithStopSource(SchedulerT& scheduler, StopSourceT source, CoroContainerT&& tasks)
    {
        std::ranges::for_each(tasks, [&source](auto& t) { t.SetStopSource(source); });

        auto futures = scheduler.Enqueue(std::forward<CoroContainerT>(tasks));
        return GetAll(futures);
    }

    template <typename StopSourceT = std::stop_source, typename SchedulerT, concepts::NonIterable... CoroTasksT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroTasksT&&... tasks)
    {
        return AnyOfWithStopSource(scheduler, StopSourceT{}, std::forward<CoroTasksT>(tasks)...);
    }

    template <typename StopSourceT = std::stop_source, typename SchedulerT, concepts::Iterable CoroContainerT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, CoroContainerT&& tasks)
    {
        return AnyOfWithStopSource(scheduler, StopSourceT{}, std::forward<CoroContainerT>(tasks));
    }

} // namespace tinycoro

#endif //!__TINY_CORO_WAIT_HPP__