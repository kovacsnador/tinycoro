#ifndef __TINY_CORO_TUPLE_UTILS_HPP__
#define __TINY_CORO_TUPLE_UTILS_HPP__

#include <tuple>

namespace tinycoro {

    template <typename T, typename U, typename... Ts>
    constexpr auto GetIndex()
    {
        if constexpr (std::same_as<T, U>)
        {
            return 0;
        }
        else
        {
            if constexpr (sizeof...(Ts))
            {
                return 1 + GetIndex<T, Ts...>();
            }
            return -1;
        }
    }

    template <typename T, typename U, typename... Us>
    constexpr auto GetIndex(const std::tuple<U, Us...>&)
    {
        return GetIndex<T, U, Us...>();
    }

    template <typename... Us>
    struct GetIndexFromTuple;

    template <typename T, typename... Us>
    struct GetIndexFromTuple<T, std::tuple<Us...>>
    {
        static constexpr int32_t value = GetIndex<T, Us...>();
    };

    template <std::size_t N, class TupleT, class NewT>
    constexpr auto ReplaceTupleElement(TupleT&& t, NewT&& n)
    {
        constexpr auto tail_size = std::tuple_size<std::remove_reference_t<TupleT>>::value - N - 1;

        return [&]<std::size_t... I_head, std::size_t... I_tail>(std::index_sequence<I_head...>, std::index_sequence<I_tail...>) {
            return std::make_tuple(std::move(std::get<I_head>(t))..., std::forward<NewT>(n), std::move(std::get<I_tail + N + 1>(t))...);
        }(std::make_index_sequence<N>{}, std::make_index_sequence<tail_size>{});
    }

} // namespace tinycoro

#endif //!__TINY_CORO_TUPLE_UTILS_HPP__