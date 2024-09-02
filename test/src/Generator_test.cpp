#include <gtest/gtest.h>

#include <ranges>

#include <tinycoro/Generator.hpp>

using TestType = std::tuple<int32_t, int32_t>;

struct GeneratorTest : testing::TestWithParam<TestType>
{
};

INSTANTIATE_TEST_SUITE_P(
    GeneratorTest,
    GeneratorTest,
    testing::Values(
        TestType{0, 10},
        TestType{-10, 20},
        TestType{100, 130}
    )
);

TEST_P(GeneratorTest, SimpleTest)
{
    auto param = GetParam();
    auto from = std::get<0>(param);
    auto to = std::get<1>(param);

    auto generator = [](int32_t from, int32_t max) -> tinycoro::Generator<int32_t> {
        for (auto it : std::views::iota(from, max))
        {
            co_yield it;
        }
    };

    int32_t count{0};

    int32_t f = from;

    for (const auto& it : generator(from, to))
    {
        EXPECT_EQ(f++, it);
        count++;
    }

    EXPECT_EQ(to - from, count);
}