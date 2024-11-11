#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/Barrier.hpp>

#include "mock/CoroutineHandleMock.h"

struct BarrierTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BarrierTest, BarrierTest, testing::Values(1, 5, 10, 100, 1000));

TEST_P(BarrierTest, BarrierTest_arrive)
{
    const auto count = GetParam();

    bool complete{false};
    auto complition = [&] { complete = true; };

    tinycoro::Barrier barrier{count, complition};

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        auto result = barrier.Arrive();
        EXPECT_EQ(result, complete);
    }

    EXPECT_TRUE(complete);
    complete = false;

    for (size_t i = 0; i < count; ++i)
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

    for (size_t i = 0; i < count; ++i)
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

    EXPECT_NO_THROW((tinycoro::Barrier{10, [] {}}));
    EXPECT_THROW((tinycoro::Barrier{0, [] {}}), tinycoro::BarrierException);
}

using DecrementData = std::tuple<int32_t, int32_t, int32_t>;

struct DecrementTest : testing::TestWithParam<DecrementData>
{
};

INSTANTIATE_TEST_SUITE_P(DecrementTest,
                         DecrementTest,
                         testing::Values(DecrementData{3, 10, 2},
                                         DecrementData{2, 10, 1},
                                         DecrementData{1, 10, 10},
                                         DecrementData{0, 10, 10},
                                         DecrementData{0, 0, 0},
                                         DecrementData{10, 0, 9},
                                         DecrementData{1, 0, 0}));

TEST_P(DecrementTest, BarrierTest_Decrement)
{
    auto [val, reset, expected] = GetParam();
    EXPECT_EQ(tinycoro::detail::local::Decrement(val, reset), expected);
}

template <typename T>
class BarrierAwaiterMock
{
public:
    BarrierAwaiterMock(auto& barrier, T, bool ready)
    {
        if (ready == false)
        {
            barrier.Add(this);
        }
    }

    MOCK_METHOD(void, Notify, ());

    BarrierAwaiterMock* next{nullptr};
};

TEST(BarrierTest, BarrierTest_coawaitReturn)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, BarrierAwaiterMock> barrier{10};

    auto awaiter = barrier.operator co_await();

    using expectedAwaiterType = BarrierAwaiterMock<tinycoro::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BarrierTest, BarrierTest_arriveAndWait)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, BarrierAwaiterMock> barrier{2};

    auto awaiter = barrier.ArriveAndWait();

    EXPECT_CALL(awaiter, Notify()).Times(1);
    barrier.Arrive();
}

struct BarrierComplitionMock
{
    struct ImplMock
    {
        MOCK_METHOD(void, Invoke, ());
    };

    BarrierComplitionMock() { mock = std::make_shared<ImplMock>(); }

    void operator()() { mock->Invoke(); }

    std::shared_ptr<ImplMock> mock;
};

TEST(BarrierTest, BarrierTest_arriveAndWait_after)
{
    BarrierComplitionMock complitionMock;

    tinycoro::Barrier barrier{2, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(1);

    barrier.Arrive();
    auto awaiter = barrier.ArriveAndWait();

    EXPECT_TRUE(awaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl([]{});
    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

TEST_P(BarrierTest, BarrierTest_complitionCallback_arrive)
{
    const size_t count = GetParam();

    BarrierComplitionMock complitionMock;

    tinycoro::Barrier barrier{count, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(2);

    for(size_t i = 0; i < count; ++i)
    {
        barrier.Arrive();
        barrier.Arrive();
    }
}

TEST(BarrierTest, BarrierTest_await_ready_and_suspend)
{
    tinycoro::Barrier barrier{2};

    auto awaiter = barrier.Wait();

    EXPECT_FALSE(awaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl([]{});
    EXPECT_TRUE(awaiter.await_suspend(hdl));
}

TEST(BarrierTest, BarrierTest_notifyAndComplition)
{
    BarrierComplitionMock complitionMock;

    tinycoro::Barrier<BarrierComplitionMock, BarrierAwaiterMock> barrier{3, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(1);

    auto awaiter = barrier.Wait();

    EXPECT_CALL(awaiter, Notify()).Times(1);

    barrier.Arrive();
    barrier.Arrive();
    barrier.Arrive();
}