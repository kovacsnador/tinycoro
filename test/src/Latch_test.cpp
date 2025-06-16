#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mock/CoroutineHandleMock.h"
#include "Allocator.hpp"

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

template <typename T, typename U>
class PopAwaiterMock : public tinycoro::detail::SingleLinkable<PopAwaiterMock<T, U>>
{
public:
    PopAwaiterMock(auto&, auto) { }
};

TEST(LatchTest, LatchTest_coawaitReturn)
{
    tinycoro::detail::Latch<PopAwaiterMock> latch{1};

    auto awaiter = latch.operator co_await();

    using expectedAwaiterType = PopAwaiterMock<decltype(latch), tinycoro::detail::PauseCallbackEvent>;
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

    EXPECT_TRUE(awaiter.await_suspend(hdl));

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

    void Set(auto func) { mock->Set(func); }

    std::shared_ptr<LatchEventMockImpl> mock;
};

struct LatchMock
{
    MOCK_METHOD(bool, Add, (void*));
};

TEST(LatchTest, LatchAwaiter)
{
    LatchMock      mock;
    LatchEventMock eventMock;

    EXPECT_CALL(mock, Add).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*eventMock.mock, Set).Times(2);

    tinycoro::detail::LatchAwaiter awaiter{mock, eventMock};

    bool pauseCalled = false;
    auto hdl         = tinycoro::test::MakeCoroutineHdl([&pauseCalled] { pauseCalled = true; });

    EXPECT_FALSE(awaiter.await_suspend(hdl));
    EXPECT_FALSE(pauseCalled);
}

struct LatchTest : testing::TestWithParam<size_t>
{
    void SetUp()
    {
        // resets the memory resource
        s_allocator.release();
    }

    static inline tinycoro::test::Allocator<LatchTest, 500000u> s_allocator;

    template<typename PromiseT>
    using Allocator = tinycoro::test::AllocatorAdapter<PromiseT, decltype(s_allocator)>;

};

INSTANTIATE_TEST_SUITE_P(LatchTest, LatchTest, testing::Values(1, 2, 10, 100, 1000, 10000));

TEST_P(LatchTest, LatchFunctionalTest_ArriveAndWait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch     latch{count};
    std::atomic<size_t> latchCount{};

    auto task = [&]() -> tinycoro::Task<void, LatchTest::Allocator> {
        co_await latch.ArriveAndWait();
        ++latchCount;
    };

    std::vector<tinycoro::Task<void, LatchTest::Allocator>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
    }

    tinycoro::GetAll(scheduler, std::move(tasks));
    EXPECT_EQ(count, latchCount);
}

TEST_P(LatchTest, LatchFunctionalTest_Wait_cancel)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;
    tinycoro::Latch     latch{count};

    auto task = [&]() -> tinycoro::Task<int32_t, LatchTest::Allocator> {
        co_await tinycoro::Cancellable(latch.Wait());
        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::Task<int32_t, LatchTest::Allocator> {
        co_await tinycoro::SleepFor<LatchTest::Allocator>(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t, LatchTest::Allocator>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
    }

    auto results = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(results[0], 44);

    for (size_t i = 1; i < results.size(); ++i)
    {
        EXPECT_FALSE(results[i].has_value());
    }
}

TEST_P(LatchTest, LatchFunctionalTest_Wait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch     latch{count};
    std::atomic<size_t> latchCount{};

    auto task = [&]() -> tinycoro::Task<void, LatchTest::Allocator> {
        co_await latch.Wait();
        ++latchCount;
    };

    auto countDown = [&]() -> tinycoro::Task<void, LatchTest::Allocator> {
        latch.CountDown();
        co_return;
    };

    std::vector<tinycoro::Task<void, LatchTest::Allocator>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
        tasks.emplace_back(countDown());
    }

    tinycoro::GetAll(scheduler, std::move(tasks));
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

    auto task = [&]() -> tinycoro::Task<void, LatchTest::Allocator> {
        ++latchCount_1;
        co_await latch_1;

        ++latchCount_2;
        co_await latch_2;
    };

    auto countDown = [&]() -> tinycoro::Task<void, LatchTest::Allocator> {
        latch_1.CountDown();
        latch_1.CountDown();

        latch_2.CountDown();
        latch_2.CountDown();
        co_return;
    };

    std::vector<tinycoro::Task<void, LatchTest::Allocator>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
        tasks.emplace_back(countDown());
    }

    tinycoro::GetAll(scheduler, std::move(tasks));
    EXPECT_EQ(count, latchCount_1);
    EXPECT_EQ(count, latchCount_2);
}

TEST_P(LatchTest, LatchTest_cancel_multi)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    tinycoro::Latch latch{count};

    std::atomic<size_t> taskCount;

    auto task1 = [&]() -> tinycoro::Task<size_t, LatchTest::Allocator> {
        co_await tinycoro::Cancellable(latch.Wait());
        co_return ++taskCount;
    };
    auto task2 = [&]() -> tinycoro::Task<size_t, LatchTest::Allocator> {
        co_await tinycoro::Cancellable(latch.ArriveAndWait());
        co_return ++taskCount;
    };

    auto sleep = [&]() -> tinycoro::Task<size_t, LatchTest::Allocator> {
        co_await tinycoro::SleepFor<LatchTest::Allocator>(clock, 50ms);
        co_return 44u;
    };

    std::vector<tinycoro::Task<size_t, LatchTest::Allocator>> tasks;
    tasks.reserve((count * 3) + 1);
    tasks.emplace_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task1());
        tasks.emplace_back(task2());
        tasks.emplace_back(task2());
    }

    auto results = tinycoro::AnyOf(scheduler, std::move(tasks));

    auto finished = std::ranges::count_if(results | std::views::drop(1), [](const auto& it) { return it.has_value(); });
    EXPECT_EQ(finished, taskCount);
}