#include <gtest/gtest.h>

#include <chrono>
#include <future>

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

    auto start  = std::chrono::system_clock::now();
    auto future = scheduler.Enqueue(tinycoro::SleepFor(clock, timeout));

    EXPECT_NO_THROW(future.get());

    EXPECT_TRUE(start + timeout <= std::chrono::system_clock::now());
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt)
{
    auto timeout1 = 200ms;
    auto timeout2 = 10000ms; // this should interrupt

    auto start  = std::chrono::system_clock::now();

    tinycoro::AnyOf(scheduler, tinycoro::SleepFor(clock, timeout1), tinycoro::SleepFor(clock, timeout2));

    auto end = std::chrono::system_clock::now();

    EXPECT_TRUE(start + timeout1 <= end);
    EXPECT_TRUE(start + timeout2 > end);
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt_custom_stopToken)
{
    auto timeout1 = 200ms;
    auto timeout2 = 10000ms; // this should interrupt

    auto start  = std::chrono::system_clock::now();

    tinycoro::AnyOf(scheduler, tinycoro::SleepForCancellable(clock, timeout1), tinycoro::SleepForCancellable(clock, timeout2));

    auto end = std::chrono::system_clock::now();

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

    auto start  = std::chrono::system_clock::now();

    tinycoro::AnyOf(scheduler, sleepTaskCancellable(timeout1), sleepTaskCancellable(timeout2));

    auto end = std::chrono::system_clock::now();

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

    auto start  = std::chrono::system_clock::now();

    tinycoro::AnyOf(scheduler, sleepTaskCancellable(timeout1), sleepTaskCancellable(timeout2));

    auto end = std::chrono::system_clock::now();

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

    auto start  = std::chrono::system_clock::now();

    tinycoro::GetAll(scheduler, stopRequester(), sleepInterruptTask(timeout1), sleepInterruptTask(timeout2));

    auto end = std::chrono::system_clock::now();

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

    auto start = std::chrono::system_clock::now();

    tinycoro::GetAll(scheduler, stopRequester(), sleepInterruptTask(timeout1));

    auto end = std::chrono::system_clock::now();

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

        auto task = [&clock](auto duration) -> tinycoro::Task<void> {
            co_await tinycoro::SleepFor(clock, duration);
        };

        EXPECT_NO_THROW(tinycoro::AnyOfWithStopSource(scheduler, source, task(1ms), task(2s), task(3s)));
    }
}