#include <gtest/gtest.h>

#include <vector>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(AutoEventTest, AutoEventTest_set)
{
    tinycoro::AutoEvent event;

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());
}

TEST(AutoEventTest, AutoEventTest_constructor)
{
    tinycoro::AutoEvent event{true};

    EXPECT_TRUE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());
}

template <typename, typename>
class AwaiterMock
{
public:
    AwaiterMock(auto&, auto) { }

    AwaiterMock* next{nullptr};
};

TEST(AutoEventTest, AutoEventTest_coawaitReturn)
{
    tinycoro::detail::AutoEvent<AwaiterMock> event;

    auto awaiter = event.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(event), tinycoro::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(AutoEventTest, AutoEventTest_await_ready)
{
    tinycoro::AutoEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());

    EXPECT_TRUE(awaiter.await_ready());

    // auto reset
    EXPECT_FALSE(event.IsSet());
    EXPECT_FALSE(awaiter.await_ready());
}

TEST(AutoEventTest, AutoEventTest_await_suspend)
{
    tinycoro::AutoEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    bool pauseCalled = false;

    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseCalled]() { pauseCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());

    EXPECT_FALSE(pauseCalled);

    EXPECT_FALSE(event.IsSet());
    event.Set();

    // auto reset
    EXPECT_FALSE(event.IsSet());
    EXPECT_TRUE(pauseCalled);
}

TEST(AutoEventTest, AutoEventTest_await_suspend_preset)
{
    tinycoro::AutoEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    event.Set();

    bool                                                            pauseCalled = false;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseCalled]() { pauseCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);

    EXPECT_FALSE(pauseCalled);

    EXPECT_FALSE(event.IsSet());
    event.Set();

    // auto reset
    EXPECT_TRUE(event.IsSet());

    // no pause pauseCalled, immediately resumed coroutine
    EXPECT_FALSE(pauseCalled);
}

struct AutoEventTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AutoEventTest, AutoEventTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(AutoEventTest, AutoEventFunctionalTest)
{
    tinycoro::Scheduler scheduler{8};

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent;
    size_t              autoCount{};

    auto autoEventConsumer = [&]() -> tinycoro::Task<void> {
        co_await autoEvent;
        autoCount++;

        autoEvent.Set();
    };

    auto autoEventProducer = [&]() -> tinycoro::Task<void> {
        // Set manually the event
        autoEvent.Set();
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }
    tasks.push_back(autoEventProducer());

    tinycoro::GetAll(scheduler, tasks);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_preset)
{
    tinycoro::Scheduler scheduler{8};

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent{true};
    size_t              autoCount{};

    auto autoEventConsumer = [&]() -> tinycoro::Task<void> {
        co_await autoEvent;
        autoCount++;

        autoEvent.Set();
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }

    tinycoro::GetAll(scheduler, tasks);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_multipleWaiters)
{
    tinycoro::Scheduler scheduler{8};
    tinycoro::AutoEvent event;
    int                 counter = 0;

    auto waiterTask = [&]() -> tinycoro::Task<void> {
        auto stopSource = co_await tinycoro::StopSourceAwaiter{};

        co_await event;

        if (stopSource.stop_requested())
        {
            event.Set();
            co_return;
        }

        stopSource.request_stop();
        ++counter;
        event.Set();
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < GetParam(); ++i)
    {
        tasks.push_back(waiterTask());
    }

    // Signal the event
    event.Set();
    tinycoro::AnyOf(scheduler, tasks);

    // Only one waiter should have been resumed
    EXPECT_EQ(counter, 1);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_concurrentSetAndWait)
{
    tinycoro::Scheduler scheduler{8};
    tinycoro::AutoEvent event{true}; // preset
    int                 counter = 0;

    auto setTask = [&]() -> tinycoro::Task<void> {
        event.Set();
        co_return;
    };

    auto task = [&]() -> tinycoro::Task<void> {
        co_await event;
        ++counter;

        // nested coroutine
        co_await setTask();
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < GetParam(); ++i)
    {
        tasks.push_back(task());
    }

    tinycoro::GetAll(scheduler, tasks);

    // Since event is auto-resetting, each Set should trigger only one waiter
    EXPECT_EQ(counter, GetParam());
}

TEST(AutoEventTest, AutoEventFunctionalTest_setBeforeAwait)
{
    tinycoro::Scheduler scheduler{8};
    tinycoro::AutoEvent event;
    bool                waited = false;

    auto task = [&]() -> tinycoro::Task<void> {
        event.Set();
        co_await event;
        waited = true;
    };

    tinycoro::GetAll(scheduler, task());

    // Since the event was set before co_await, waited should be true
    EXPECT_TRUE(waited);
}