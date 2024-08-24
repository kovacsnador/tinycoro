#ifndef __TINY_CORO_WAIT_HPP__
#define __TINY_CORO_WAIT_HPP__

#include <algorithm>
#include <variant>
#include <tuple>
#include <vector>
#include <exception>
#include <type_traits>
#include <concepts>

namespace tinycoro {

    template <template <typename> class FutureT, typename... Ts>
    [[nodiscard]] auto WaitAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto waiter = []<typename T>(FutureT<T>& f) {
            if constexpr (std::same_as<void, T>)
            {
                using var_t = std::variant<std::monostate, std::exception_ptr>;

                try
                {
                    f.get();
                    return var_t{std::monostate{}};
                }
                catch(...)
                {
                    return var_t{std::current_exception()};
                }
            }
            else
            {
                using var_t = std::variant<T, std::exception_ptr>;
                try
                {
                    return var_t{f.get()};
                }
                catch(...)
                {
                    return var_t{std::current_exception()};
                }
            }
        };

        return std::apply([waiter]<typename... TypesT>(TypesT&... args) { return std::make_tuple(waiter(args)...); }, futures);
    }

    template <template <typename> class FutureT, typename ReturnT>
    [[nodiscard]] auto WaitAll(std::vector<FutureT<ReturnT>>& futures)
    {
        using ValueT = std::conditional_t<std::same_as<void, ReturnT>, std::monostate, ReturnT>;
        using var_t = std::variant<ValueT, std::exception_ptr>;
        std::vector<var_t> results;
        results.reserve(futures.size());

        for (auto& it : futures)
        {
            try
            {
                if constexpr (std::same_as<void, ReturnT>)
                {
                    it.get();
                    results.emplace_back(std::monostate{});
                }   
                else
                {
                    results.emplace_back(std::move(it.get()));
                }
            }
            catch(...)
            {
                results.emplace_back(std::current_exception());
            }
        }

        return results;
    }

    template <typename SchedulerT, typename StopSourceT, typename... CoroTasksT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, StopSourceT source, CoroTasksT&&... tasks)
    {
        (tasks.SetStopSource(source), ...);

        auto futures = scheduler.EnqueueTasks(std::forward<CoroTasksT>(tasks)...);
        return WaitAll(futures);
    }

    template <typename SchedulerT, typename StopSourceT, typename CoroContainerT>
    [[nodiscard]] auto AnyOf(SchedulerT& scheduler, StopSourceT source, CoroContainerT&& tasks)
    {
        std::ranges::for_each(tasks, [&source](auto& t){ t.SetStopSource(source); });

        auto futures = scheduler.EnqueueTasks(std::forward<CoroContainerT>(tasks));
        return WaitAll(futures);
    }

} // namespace tinycoro

#endif //!__TINY_CORO_WAIT_HPP__