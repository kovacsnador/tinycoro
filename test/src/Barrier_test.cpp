#include <gtest/gtest.h>

#include <tinycoro/Barrier.hpp>

struct BarrierTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(
    BarrierTest,
    BarrierTest,
    testing::Values(1, 5, 10, 100, 1000)
);

TEST_P(BarrierTest, BarrierTest_arrive)
{
    const auto count = GetParam();

    bool complete{false};
    auto complition = [&] { complete = true; };

    tinycoro::Barrier barrier{count, complition};

    for(size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        auto result = barrier.Arrive();
        EXPECT_EQ(result, complete);
    }

    EXPECT_TRUE(complete);
    complete = false;

    for(size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        auto result = barrier.Arrive();
        EXPECT_EQ(result, complete);
    }

    EXPECT_TRUE(complete);
}

TEST_P(BarrierTest, BarrierTest_arriveAndDrop)
{
    const auto count = GetParam();

    bool complete{false};
    auto complition = [&] { complete = true; };

    tinycoro::Barrier barrier{count, complition};

    for(size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        barrier.ArriveAndDrop();
    }

    EXPECT_TRUE(complete);
    complete = false;

    // total count should be 0 here, so no suspend any more
    EXPECT_TRUE(barrier.Arrive());
    EXPECT_TRUE(complete);
}

TEST(BarrierTest, BarrierTest_constructor)
{
    tinycoro::Barrier barrier{10};

    /*EXPECT_NO_THROW(tinycoro::Barrier(10));
    EXPECT_THROW(tinycoro::Barrier barrier(0));

    EXPECT_NO_THROW(tinycoro::Barrier barrier(10, []{}));
    EXPECT_THROW(tinycoro::Barrier barrier(0, []{}));*/
}