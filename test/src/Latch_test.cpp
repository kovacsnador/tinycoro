#include <gtest/gtest.h>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(LatchTest, LatchTest_countdown)
{
    tinycoro::Latch latch{4};

    auto awaiter = latch.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());
    latch.CountDown();

    EXPECT_FALSE(awaiter.await_ready());
    latch.CountDown();

    EXPECT_FALSE(awaiter.await_ready());
    latch.CountDown();

    EXPECT_FALSE(awaiter.await_ready());
    latch.CountDown();

    EXPECT_TRUE(awaiter.await_ready());
}

TEST(LatchTest, LatchTest_ArriveAndWait)
{
    tinycoro::Latch latch{4};

    auto awaiter = latch.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());
    auto awaiter1 = latch.ArriveAndWait();
    EXPECT_TRUE((std::same_as<decltype(awaiter1), decltype(awaiter)>));

    EXPECT_FALSE(awaiter.await_ready());
    auto awaiter2 = latch.ArriveAndWait();
    EXPECT_TRUE((std::same_as<decltype(awaiter2), decltype(awaiter)>));

    EXPECT_FALSE(awaiter.await_ready());
    auto awaiter3 = latch.ArriveAndWait();
    EXPECT_TRUE((std::same_as<decltype(awaiter3), decltype(awaiter)>));

    EXPECT_FALSE(awaiter.await_ready());
    auto awaiter4 = latch.ArriveAndWait();
    EXPECT_TRUE((std::same_as<decltype(awaiter4), decltype(awaiter)>));

    EXPECT_TRUE(awaiter.await_ready());
}

TEST(LatchTest, LatchTest_constructor)
{
    EXPECT_NO_THROW(tinycoro::Latch{4});
    EXPECT_NO_THROW(tinycoro::Latch{1});
    EXPECT_NO_THROW(tinycoro::Latch{1000});

    EXPECT_THROW(tinycoro::Latch{0}, tinycoro::LatchException);
}

template <typename, typename>
struct AwaiterMock
{
    AwaiterMock(auto&, auto) { }

    AwaiterMock* next{nullptr};
};

TEST(LatchTest, LatchTest_coawaitReturn)
{
    tinycoro::detail::Latch<AwaiterMock> latch{1};

    auto awaiter = latch.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(latch), tinycoro::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(LatchTest, LatchTest_await_ready)
{
    tinycoro::Latch latch{1};

    auto awaiter = latch.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());
    latch.CountDown();
    EXPECT_TRUE(awaiter.await_ready());
}

TEST(LatchTest, LatchTest_await_suspend)
{
    tinycoro::Latch latch{3};

    auto awaiter = latch.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    bool pauseCalled = false;
    auto hdl         = tinycoro::test::MakeCoroutineHdl([&pauseCalled] { pauseCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());

    EXPECT_FALSE(pauseCalled);
    latch.CountDown();
    latch.CountDown();
    latch.CountDown();
    EXPECT_TRUE(pauseCalled);
}

struct LatchTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(LatchTest, LatchTest, testing::Values(1, 2, 10, 100, 1000));

TEST_P(LatchTest, LatchFunctionalTest_ArriveAndWait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch     latch{count};
    std::atomic<size_t> latchCount{};

    auto task = [&]() -> tinycoro::Task<void> {
        co_await latch.ArriveAndWait();
        ++latchCount;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
    }

    tinycoro::GetAll(scheduler, tasks);
    EXPECT_EQ(count, latchCount);
}

TEST_P(LatchTest, LatchFunctionalTest_Wait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch     latch{count};
    std::atomic<size_t> latchCount{};

    auto task = [&]() -> tinycoro::Task<void> {
        co_await latch.Wait();
        ++latchCount;
    };

    auto countDown = [&]()->tinycoro::Task<void>
    {
        latch.CountDown();
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
        tasks.emplace_back(countDown());
    }

    tinycoro::GetAll(scheduler, tasks);
    EXPECT_EQ(count, latchCount);
}