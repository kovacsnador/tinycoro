#include <gtest/gtest.h>

#include "tinycoro/SchedulableTask.hpp"

TEST(FutureTypeGetterTest, FutureTypeGetterTest_void)
{
    using getter_t = tinycoro::detail::FutureTypeGetter<void, std::promise>;

    EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, getter_t::futureReturn_t>));
    EXPECT_TRUE((std::same_as<std::promise<std::optional<tinycoro::VoidType>>, getter_t::futureState_t>));
    EXPECT_TRUE((std::same_as<std::future<std::optional<tinycoro::VoidType>>, getter_t::future_t>));
}

TEST(FutureTypeGetterTest, FutureTypeGetterTest_int32_t)
{
    using getter_t = tinycoro::detail::FutureTypeGetter<int32_t, std::promise>;

    EXPECT_TRUE((std::same_as<std::optional<int32_t>, getter_t::futureReturn_t>));
    EXPECT_TRUE((std::same_as<std::promise<std::optional<int32_t>>, getter_t::futureState_t>));
    EXPECT_TRUE((std::same_as<std::future<std::optional<int32_t>>, getter_t::future_t>));
}