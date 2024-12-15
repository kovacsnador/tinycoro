#include <gtest/gtest.h>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(ManualEventTest, ManualEventTest_set_reset)
{
    tinycoro::ManualEvent event;

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());

    event.Reset();

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());
}

template <typename, typename>
class AwaiterMock
{
public:
    AwaiterMock(auto&, auto) { }
};

TEST(ManualEventTest, ManualEventTest_coawaitReturn)
{
    tinycoro::detail::ManualEvent<AwaiterMock> event;

    auto awaiter = event.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(event), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(ManualEventTest, ManualEventTest_await_ready)
{
    tinycoro::ManualEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());

    EXPECT_TRUE(awaiter.await_ready());

    event.Reset();
    EXPECT_FALSE(awaiter.await_ready());
}

TEST(ManualEventTest, ManualEventTest_already_ready_await_ready)
{
    tinycoro::ManualEvent event;

    auto awaiter = event.operator co_await();

    event.Set();
    EXPECT_TRUE(awaiter.await_ready());
}

TEST(ManualEventTest, ManualEventTest_await_suspend)
{
    tinycoro::ManualEvent event;

    auto awaiter = event.operator co_await();

    bool                                                         pauseResumerCalled = false;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<void>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseResumerCalled]() { pauseResumerCalled = true; });

    event.Set();
    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);
}

TEST(ManualEventTest, ManualEventTest_await_suspend_singleConsumer)
{
    tinycoro::ManualEvent event;

    auto awaiter1 = event.operator co_await();

    EXPECT_FALSE(awaiter1.await_ready());

    bool                                                         pauseResumerCalled = false;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<void>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseResumerCalled]() { pauseResumerCalled = true; });

    EXPECT_EQ(awaiter1.await_suspend(hdl), std::noop_coroutine());
    EXPECT_FALSE(pauseResumerCalled);

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(pauseResumerCalled);
    EXPECT_TRUE(event.IsSet());
}

TEST(ManualEventTest, ManualEventTest_await_suspend_multiConsumer)
{
    tinycoro::ManualEvent event;

    auto awaiter1 = event.operator co_await();
    EXPECT_FALSE(awaiter1.await_ready());

    auto awaiter2 = event.operator co_await();
    EXPECT_FALSE(awaiter2.await_ready());

    auto awaiter3 = event.operator co_await();
    EXPECT_FALSE(awaiter3.await_ready());

    size_t pauseResumerCalled   = 0;
    auto   pauseResumerCallback = [&pauseResumerCalled]() { ++pauseResumerCalled; };

    auto makeHdl = [&]() {
        tinycoro::test::CoroutineHandleMock<tinycoro::Promise<void>> hdl;
        hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>(pauseResumerCallback);
        return hdl;
    };

    EXPECT_EQ(awaiter1.await_suspend(makeHdl()), std::noop_coroutine());
    EXPECT_EQ(awaiter2.await_suspend(makeHdl()), std::noop_coroutine());
    EXPECT_EQ(awaiter3.await_suspend(makeHdl()), std::noop_coroutine());

    EXPECT_EQ(pauseResumerCalled, 0);

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_EQ(pauseResumerCalled, 3);
    EXPECT_TRUE(event.IsSet());
}

struct ManualEventTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(ManualEventTest, ManualEventTest, testing::Values(1, 10, 100, 1000));

TEST_P(ManualEventTest, ManualEventFunctionalTest)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{4};

    tinycoro::ManualEvent event;

    std::atomic<size_t> globalCount{};

    auto trigger = [&]() -> tinycoro::Task<void> {
        event.Set();
        co_return;
    };

    auto listener = [&]() -> tinycoro::Task<void> {
        co_await event;
        ++globalCount;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(listener());
    }
    tasks.push_back(trigger());

    tinycoro::GetAll(scheduler, tasks);

    EXPECT_EQ(globalCount, count);
}

TEST_P(ManualEventTest, ManualEventFunctionalTest_preSet)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{4};

    tinycoro::ManualEvent event;

    std::atomic<size_t> globalCount{};

    auto listener = [&]() -> tinycoro::Task<void> {
        co_await event;
        ++globalCount;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(listener());
    }

    // preset the event
    event.Set();

    tinycoro::GetAll(scheduler, tasks);

    EXPECT_EQ(globalCount, count);
}

struct ManualEventStressTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(ManualEventStressTest, ManualEventStressTest, testing::Values(1, 10, 100, 1000, 10'000, 100'000));

TEST_P(ManualEventStressTest, ManualEventStressTest_1)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    tinycoro::ManualEvent event;

    std::atomic<size_t> globalCount{};

    auto producer = [&]() -> tinycoro::Task<void> {
        while (++globalCount < count)
        {
            event.Set();
        }
        co_return;
    };

    auto task = [&]() -> tinycoro::Task<void> {
        while (globalCount < count)
        {
            co_await event;
        }
    };

    tinycoro::GetAll(scheduler, task(), producer(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(globalCount, count);
}