#ifndef __TINY_CORO_WAIT_HPP__
#define __TINY_CORO_WAIT_HPP__

namespace tinycoro {

    namespace concepts {

        template <typename T, typename... Ts>
        concept AllSame = (std::same_as<T, Ts> && ...);

    } // namespace concepts

    template <template <typename> class FutureT, typename... Ts>
        requires (!concepts::AllSame<void, Ts...>)
    [[nodiscard]] auto WaitAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto waiter = []<typename T>(FutureT<T>& f) {
            if constexpr (std::same_as<void, T>)
            {
                f.get();
                return std::monostate{};
            }
            else
            {
                return f.get();
            }
        };

        return std::apply([waiter]<typename... TypesT>(TypesT&... args) { return std::make_tuple(waiter(args)...); }, futures);
    }

    template <template <typename> class FutureT, typename... Ts>
        requires concepts::AllSame<void, Ts...>
    void WaitAll(std::tuple<FutureT<Ts>...>& futures)
    {
        auto futureGet = [](auto& fut) {
            if (fut.valid())
            {
                fut.get();
            }
        };
        std::apply([futureGet](auto&... future) { ((futureGet(future)), ...); }, futures);
    }

    template <template <typename> class FutureT, typename ReturnT>
        requires (!std::same_as<void, ReturnT>)
    [[nodiscard]] std::vector<ReturnT> WaitAll(std::vector<FutureT<ReturnT>>& futures)
    {
        std::vector<ReturnT> results;
        results.reserve(futures.size());

        for (auto& it : futures)
        {
            results.emplace_back(std::move(it.get()));
        }

        return results;
    }

    template <template <typename> class FutureT, typename ReturnT>
        requires std::same_as<void, ReturnT>
    void WaitAll(std::vector<FutureT<ReturnT>>& futures)
    {
        for (auto& it : futures)
        {
            it.get();
        }
    }

} // namespace tinycoro

#endif //!__TINY_CORO_WAIT_HPP__