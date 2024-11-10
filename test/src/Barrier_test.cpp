#include <gtest/gtest.h>
#include <gmock/gmock.h>

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
    EXPECT_NO_THROW(tinycoro::Barrier{10});
    EXPECT_THROW(tinycoro::Barrier{0}, tinycoro::BarrierException);

    EXPECT_NO_THROW((tinycoro::Barrier{10, []{}}));
    EXPECT_THROW((tinycoro::Barrier{0, []{}}), tinycoro::BarrierException);
}

using DecrementData = std::tuple<int32_t, int32_t,int32_t>;

struct DecrementTest : testing::TestWithParam<DecrementData>
{
};

INSTANTIATE_TEST_SUITE_P(
    DecrementTest,
    DecrementTest,
    testing::Values(
        DecrementData{3, 10, 2},
        DecrementData{2, 10, 1},
        DecrementData{1, 10, 10},
        DecrementData{0, 10, 10},
        DecrementData{0, 0, 0},
        DecrementData{10, 0, 9},
        DecrementData{1, 0, 0}
    )
);

TEST_P(DecrementTest, BarrierTest_Decrement)
{
    auto [val, reset, expected] = GetParam();
    EXPECT_EQ(tinycoro::detail::local::Decrement(val, reset), expected);
}

template <typename>
class AwaiterMock
{
public:
    AwaiterMock(auto, auto) { }

    //MOCK_METHOD(void, Notify, ());

    void Notify() { notifyCalled=true; }

    AwaiterMock* next{nullptr};

    bool notifyCalled{false};
};

TEST(BarrierTest, BarrierTest_coawaitReturn)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, AwaiterMock> barrier{10};

    auto awaiter = barrier.operator co_await();

    using expectedAwaiterType = AwaiterMock<tinycoro::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BarrierTest, BarrierTest_arriveAndWait)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, AwaiterMock> barrier{2};

    auto awaiter = barrier.ArriveAndWait();
    barrier.Arrive();

    EXPECT_TRUE(awaiter.notifyCalled);
}