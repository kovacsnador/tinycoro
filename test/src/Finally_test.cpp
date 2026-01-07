#include <gtest/gtest.h>

#include <tinycoro/Finally.hpp>

TEST(FinallyTest, FinallyTest_SimpleTest)
{
    bool done{false};

    {
        auto action = tinycoro::Finally([&done] { done = true; });
    }

    EXPECT_TRUE(done);
}

TEST(FinallyTest, FinallyTest_MoveTest)
{
    uint32_t count{0};

    {
        auto             action = tinycoro::Finally([&count] { ++count; });
        decltype(action) action2{std::move(action)};
    }

    EXPECT_EQ(count, 1);
}

TEST(FinallyTest, FinallyTest_FunctionTest)
{
    uint32_t count{0};

    auto lambda = [&count] {
        auto action = tinycoro::Finally([&count] { ++count; });
        return 42;
    };

    auto res = lambda();

    EXPECT_EQ(res, 42);
    EXPECT_EQ(count, 1);
}

TEST(FinallyTest, FinallyTest_empty_std_function)
{
    {
        // This function is not callable,
        // we should not call this function.
        auto action = tinycoro::Finally(std::function<void()>{});
    }

    SUCCEED();
}