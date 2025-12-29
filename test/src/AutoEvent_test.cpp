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

template <typename T, typename U>
class PopAwaiterMock : tinycoro::detail::SingleLinkable<PopAwaiterMock<T, U>>
{
public:
    PopAwaiterMock(auto&, auto) { }
};

TEST(AutoEventTest, AutoEventTest_coawaitReturn)
{
    tinycoro::detail::AutoEvent<PopAwaiterMock> event;

    auto awaiter = event.operator co_await();

    using expectedAwaiterType = PopAwaiterMock<decltype(event), tinycoro::detail::ResumeSignalEvent>;
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
    auto hdl = tinycoro::test::MakeCoroutineHdl<int32_t>([&pauseCalled]([[maybe_unused]] auto policy) { pauseCalled = true; });


    EXPECT_TRUE(awaiter.await_suspend(hdl));

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

    bool pauseCalled = false;
    auto hdl = tinycoro::test::MakeCoroutineHdl<int32_t>([&pauseCalled]([[maybe_unused]] auto policy) { pauseCalled = true; });

    EXPECT_FALSE(awaiter.await_suspend(hdl));

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

    tinycoro::AllOf(scheduler, std::move(tasks));
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

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, autoCount);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_set_and_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent{true};

    auto autoEventConsumer = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(autoEvent.Wait());
        autoEvent.Set();
        co_return 42;
    };

    auto sleep = [&]()->tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 50ms);
        co_return 44;
    };  

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.push_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }

    auto results = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(results[0], 44);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent;
    int32_t              autoCount{};

    auto autoEventConsumer = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable{autoEvent.Wait()};
        co_return autoCount++;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return ++autoCount;
    };

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.push_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }

    auto results = tinycoro::AnyOf(scheduler, std::move(tasks));

    for(size_t i = 1; i < count + 1; ++i)
    {
        EXPECT_FALSE(results[i].has_value());
    }

    EXPECT_EQ(results[0], 1);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_cancel_AnyOf)
{
    tinycoro::SoftClock clock;

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent;
    int32_t              autoCount{};

    auto autoEventConsumer = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(autoEvent.Wait());
        co_return autoCount++;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return ++autoCount;
    };

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.push_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }

    auto results = tinycoro::AnyOf(tasks);

    EXPECT_EQ(results[0], 1);

    for(size_t i = 1; i <= count; ++i)
    {
        EXPECT_FALSE(results[i].has_value());
    }
}

struct AutoEventTimeoutTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AutoEventTimeoutTest, AutoEventTimeoutTest, testing::Values(1, 10, 100, 1000));

TEST_P(AutoEventTimeoutTest, AutoEventFunctionalTest_timeout)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent;
    std::atomic<int32_t> doneCount{};

    auto autoEventConsumer = [&]() -> tinycoro::TaskNIC<> {
        std::ignore = co_await tinycoro::TimeoutAwait{clock, autoEvent.Wait(), 20ms};
        doneCount++;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<> {
        co_await tinycoro::SleepFor(clock, 20ms);
        autoEvent.Set();
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count + 1);
    tasks.push_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }
    
    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(doneCount, count);
}

TEST_P(AutoEventTimeoutTest, AutoEventFunctionalTest_all_timeout)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto count = GetParam();

    tinycoro::AutoEvent autoEvent;
    std::atomic<int32_t> doneCount{};

    auto autoEventConsumer = [&]() -> tinycoro::TaskNIC<> {
        std::ignore = co_await tinycoro::TimeoutAwait{clock, autoEvent.Wait(), 10ms};
        doneCount++;
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(autoEventConsumer());
    }
    
    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(doneCount, count);
}

TEST_P(AutoEventTest, AutoEventFunctionalTest_multipleWaiters)
{
    tinycoro::Scheduler scheduler{8};
    tinycoro::AutoEvent event;
    int                 counter = 0;

    auto waiterTask = [&]() -> tinycoro::Task<void> {
        auto stopSource = co_await tinycoro::this_coro::stop_source();

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
    tinycoro::AnyOf(scheduler, std::move(tasks));

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

    tinycoro::AllOf(scheduler, std::move(tasks));

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

    tinycoro::AllOf(scheduler, task());

    // Since the event was set before co_await, waited should be true
    EXPECT_TRUE(waited);
}

struct AutoEventStressTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AutoEventStressTest, AutoEventStressTest, testing::Values(100, 1'000, 10'000));

TEST_P(AutoEventStressTest, AutoEventStressTest_1)
{
    tinycoro::AutoEvent autoEvent{true};

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto size = GetParam();

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < size; ++i)
        {
            co_await autoEvent;
            ++count;
            autoEvent.Set();
        }
    };

    // starting 8 async tasks at the same time
    tinycoro::AllOf(scheduler, task(), task(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(count, size * 8);
}