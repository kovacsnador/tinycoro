#include <gtest/gtest.h>

#include <vector>
#include <list>
#include <array>
#include <map>
#include <type_traits>
#include <concepts>
#include <tuple>

#include <tinycoro/Common.hpp>

template<typename T>
struct Concepts_IterableTest : public testing::Test
{
    using value_type = T;
};

using IterableTypes = testing::Types<std::tuple<std::vector<int>, std::true_type>,
                                         std::tuple<std::list<int>, std::true_type>,
                                          std::tuple<std::array<int, 10>, std::true_type>,
                                           std::tuple<std::map<int, int>, std::true_type>,
                                           std::tuple<int, std::false_type>,
                                           std::tuple<float, std::false_type>,
                                           std::tuple<std::monostate, std::false_type>>;

TYPED_TEST_SUITE(Concepts_IterableTest, IterableTypes);

TYPED_TEST(Concepts_IterableTest, Concepts_IterableTest_Positive)
{
    using T = typename TestFixture::value_type;

    using firstParamT = std::decay_t<decltype(std::get<0>(std::declval<T>()))>;
    using secondParamT = std::decay_t<decltype(std::get<1>(std::declval<T>()))>;

    if constexpr (std::same_as<secondParamT, std::true_type>)
    {
        if constexpr (!tinycoro::concepts::Iterable<firstParamT>) {
            EXPECT_FALSE(true);   
        }
    }
    else
    {
        if constexpr (tinycoro::concepts::Iterable<firstParamT>) {
            EXPECT_FALSE(true);   
        }
    }
}