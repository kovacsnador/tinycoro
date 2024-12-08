#include <gtest/gtest.h>
#include <gmock/gmock.h>

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
class AwaiterMock
{
public:
    AwaiterMock(auto&, auto) { }

    AwaiterMock* next{nullptr};
};

TEST(LatchTest, LatchTest_coawaitReturn)
{
    tinycoro::detail::Latch<AwaiterMock> latch{1};

    auto awaiter = latch.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(latch), tinycoro::detail::PauseCallbackEvent>;
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

struct LatchEventMockImpl
{
    MOCK_METHOD(void, Set, (std::function<void()>));
};

struct LatchEventMock
{
    LatchEventMock()
    : mock{std::make_shared<LatchEventMockImpl>()}
    {
    }

    void Set(auto func)
    {
        mock->Set(func);
    }

    std::shared_ptr<LatchEventMockImpl> mock;
};

struct LatchMock
{
    MOCK_METHOD(bool, Add, (void*));
};

TEST(LatchTest, LatchAwaiter)
{
    LatchMock mock;
    LatchEventMock eventMock;

    EXPECT_CALL(mock, Add).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*eventMock.mock, Set).Times(2);

    tinycoro::detail::LatchAwaiter awaiter{mock, eventMock};

    bool pauseCalled = false;
    auto hdl         = tinycoro::test::MakeCoroutineHdl([&pauseCalled] { pauseCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);
    EXPECT_FALSE(pauseCalled);
}

struct LatchTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(LatchTest, LatchTest, testing::Values(1, 2, 10, 100, 1000, 10000));

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

TEST_P(LatchTest, LatchFunctionalTest_coawait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch     latch_1{count};
    tinycoro::Latch     latch_2{count};
    std::atomic<size_t> latchCount_1{};
    std::atomic<size_t> latchCount_2{};

    auto task = [&]() -> tinycoro::Task<void> {
        ++latchCount_1;
        co_await latch_1;

        ++latchCount_2;
        co_await latch_2;
    };

    auto countDown = [&]()->tinycoro::Task<void>
    {
        latch_1.CountDown();
        latch_1.CountDown();

        latch_2.CountDown();
        latch_2.CountDown();
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
        tasks.emplace_back(countDown());
    }

    tinycoro::GetAll(scheduler, tasks);
    EXPECT_EQ(count, latchCount_1);
    EXPECT_EQ(count, latchCount_2);
}