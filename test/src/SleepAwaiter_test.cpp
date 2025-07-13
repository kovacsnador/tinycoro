#include <gtest/gtest.h>

#include <chrono>
#include <future>

#include "Allocator.hpp"

#include <tinycoro/tinycoro_all.h>

using namespace std::chrono_literals;

struct SleepAwaiterTest : testing::Test
{
    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler{4};
};

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest)
{
    auto timeout = 200ms;

    auto start  = tinycoro::SoftClock::Now();
    auto future = scheduler.Enqueue(tinycoro::SleepFor(clock, timeout));

    EXPECT_NO_THROW(future.get());

    EXPECT_TRUE(start + timeout <= tinycoro::SoftClock::Now());
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt)
{
    auto timeout1 = 200ms;
    auto timeout2 = 10000ms; // this should interrupt

    auto start = tinycoro::SoftClock::Now();

    tinycoro::AnyOf(scheduler, tinycoro::SleepFor(clock, timeout1), tinycoro::SleepFor(clock, timeout2));

    auto end = tinycoro::SoftClock::Now();

    EXPECT_TRUE(start + timeout1 <= end);
    EXPECT_TRUE(start + timeout2 > end);
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt_custom_stopToken)
{
    auto timeout1 = 200ms;
    auto timeout2 = 10000ms; // this should interrupt

    auto start = tinycoro::SoftClock::Now();

    tinycoro::AnyOf(scheduler, tinycoro::SleepForCancellable(clock, timeout1), tinycoro::SleepForCancellable(clock, timeout2));

    auto end = tinycoro::SoftClock::Now();

    EXPECT_TRUE(start + timeout1 <= end);
    EXPECT_TRUE(start + timeout2 > end);
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_cancellable)
{
    std::atomic<int32_t> count{};

    auto sleepTaskCancellable = [&](auto duration) -> tinycoro::Task<void> {
        co_await tinycoro::SleepForCancellable(clock, duration);
        ++count;
    };

    auto timeout1 = 200ms;
    auto timeout2 = 10000ms; // this should interrupt

    auto start  = tinycoro::SoftClock::Now();

    tinycoro::AnyOf(scheduler, sleepTaskCancellable(timeout1), sleepTaskCancellable(timeout2));

    auto end = tinycoro::SoftClock::Now();

    EXPECT_TRUE(start + timeout1 <= end);
    EXPECT_TRUE(start + timeout2 > end);

    EXPECT_EQ(count, 1);
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_cancellable_custom_stopToken)
{
    std::atomic<int32_t> count{};

    auto sleepTaskCancellable = [&](auto duration) -> tinycoro::Task<void> {

        auto stopToken = co_await tinycoro::StopTokenAwaiter{};
        co_await tinycoro::SleepForCancellable(clock, duration, stopToken);
        ++count;
    };

    auto timeout1 = 200ms;
    auto timeout2 = 10000ms; // this should interrupt

    auto start  = tinycoro::SoftClock::Now();

    tinycoro::AnyOf(scheduler, sleepTaskCancellable(timeout1), sleepTaskCancellable(timeout2));

    auto end = tinycoro::SoftClock::Now();

    EXPECT_TRUE(start + timeout1 <= end);
    EXPECT_TRUE(start + timeout2 > end);

    EXPECT_EQ(count, 1);
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt_sleep)
{
    std::atomic<int32_t> count{};
    std::stop_source stopSource;

    auto sleepInterruptTask = [&](auto duration) -> tinycoro::Task<void> {
        co_await tinycoro::SleepFor(clock, duration, stopSource.get_token());
        ++count;
    };

    auto stopRequester = [&]() -> tinycoro::Task<void> {
        stopSource.request_stop();
        co_return;
    };

    auto timeout1 = 2s;
    auto timeout2 = 10s;

    auto start  = tinycoro::SoftClock::Now();

    tinycoro::AllOf(scheduler, stopRequester(), sleepInterruptTask(timeout1), sleepInterruptTask(timeout2));

    auto end = tinycoro::SoftClock::Now();

    EXPECT_TRUE(start + timeout1 > end);
    EXPECT_TRUE(start + timeout2 > end);

    // both sould interrupt the sleep
    // (but don't cancel the parent coroutine)
    // and increase the value
    EXPECT_EQ(count, 2);
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt_sleep_cancellable)
{
    std::atomic<int32_t> count{};
    std::stop_source stopSource;

    tinycoro::AutoEvent event;

    auto sleepInterruptTask = [&](auto duration) -> tinycoro::Task<void> {

        // save the stop source in the global variable
        stopSource = co_await tinycoro::this_coro::stop_source();

        event.Set();

        co_await tinycoro::SleepForCancellable(clock, duration);
        ++count;
    };

    auto stopRequester = [&]() -> tinycoro::Task<void> {
        co_await event;
        
        // request a stop
        stopSource.request_stop();
        co_return;
    };

    auto timeout1 = 2s;

    auto start = tinycoro::SoftClock::Now();

    tinycoro::AllOf(scheduler, stopRequester(), sleepInterruptTask(timeout1));

    auto end = tinycoro::SoftClock::Now();

    EXPECT_TRUE(start + timeout1 > end);

    // both sould interrupt the sleep
    // (but don't cancel the parent coroutine)
    // and increase the value
    EXPECT_EQ(count, 0);
}

TEST(IsDurationTest, IsDurationTest)
{
    EXPECT_TRUE((tinycoro::concepts::IsDuration<std::chrono::milliseconds>));
    EXPECT_TRUE((tinycoro::concepts::IsDuration<std::chrono::seconds>));
    EXPECT_TRUE((tinycoro::concepts::IsDuration<std::chrono::hours>));

    EXPECT_TRUE((tinycoro::concepts::IsDuration<const std::chrono::milliseconds>));
    EXPECT_TRUE((tinycoro::concepts::IsDuration<const std::chrono::seconds>));
    EXPECT_TRUE((tinycoro::concepts::IsDuration<const std::chrono::hours>));

    EXPECT_TRUE((tinycoro::concepts::IsDuration<const volatile std::chrono::milliseconds>));
    EXPECT_TRUE((tinycoro::concepts::IsDuration<const volatile std::chrono::seconds>));
    EXPECT_TRUE((tinycoro::concepts::IsDuration<const volatile std::chrono::hours>));

    if constexpr (tinycoro::concepts::IsDuration<int>)
    {
        EXPECT_FALSE(true);
    }
}

struct SleepAwaiterStressTest : testing::TestWithParam<size_t>
{
    void SetUp() override
    {
        // resets the memory resource
        s_allocator.release();
    }

    static inline tinycoro::test::Allocator<SleepAwaiterStressTest, 100000u> s_allocator;

    template<typename PromiseT>
    using Allocator = tinycoro::test::AllocatorAdapter<PromiseT, decltype(s_allocator)>;
};

INSTANTIATE_TEST_SUITE_P(SleepAwaiterStressTest, SleepAwaiterStressTest, testing::Values(1, 10, 100));

TEST_P(SleepAwaiterStressTest, SleepAwaiterStressTest_sleepFor)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler;

    for (size_t i = 0; i < count; ++i)
    {
        std::stop_source source;

        auto task = [&clock](auto duration) -> tinycoro::Task<void, SleepAwaiterStressTest::Allocator> {
            co_await tinycoro::SleepFor<SleepAwaiterStressTest::Allocator>(clock, duration);
        };

        EXPECT_NO_THROW(tinycoro::AnyOf(scheduler, source, task(1ms), task(2s), task(3s)));
    }
}