#include <gtest/gtest.h>
#include <concepts>

#include "tinycoro/UnsafeFunction.hpp"


TEST(UnsafeFunctionTest, UnsafeFunctionTest_constructor)
{
    auto f = [](int32_t val) { EXPECT_EQ(val, 42); };

    tinycoro::detail::UnsafeFunction<void(int32_t)> function{f, 42};

    // invoke the function
    function();
}

TEST(UnsafeFunctionTest, UnsafeFunctionTest_assignment)
{
    auto f = [](int32_t val) { EXPECT_EQ(val, 42); };

    tinycoro::detail::UnsafeFunction<void(int32_t)> function{f, 44};
    function = {f, 42};

    // invoke the function
    function();
}

TEST(UnsafeFunctionTest, UnsafeFunctionTest_return_value)
{
    auto f = [](int32_t val) { EXPECT_EQ(val, 42); return val; };

    tinycoro::detail::UnsafeFunction<int32_t(int32_t)> function{f, 44};
    function = {f, 42};

    // invoke the function
    auto ret = function();
    EXPECT_EQ(ret, 42);
}

TEST(UnsafeFunctionTest, UnsafeFunctionTest_regular_invocable)
{
    auto f = [](int32_t val) { EXPECT_EQ(val, 42); return val; };

    tinycoro::detail::UnsafeFunction<int32_t(int32_t)> function{f, 42};
    
    EXPECT_TRUE((std::regular_invocable<decltype(function)>));
    EXPECT_TRUE((std::invocable<decltype(function)>));
}