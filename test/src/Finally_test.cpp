#include <gtest/gtest.h>

#include <tinycoro/Finally.hpp>

TEST(FinallyTest, FinallyTest_SimpleTest)
{
    bool done{false};

    {
        auto action = tinycoro::Finally([&done]{ done = true; });
    }

    EXPECT_TRUE(done);
}

TEST(FinallyTest, FinallyTest_MoveTest)
{
    uint32_t count{0};

    {
        auto action = tinycoro::Finally([&count]{ ++count; });
        decltype(action) action2{std::move(action)};
    }

    EXPECT_EQ(count, 1);
}