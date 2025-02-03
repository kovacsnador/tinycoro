#include <gtest/gtest.h>

#include <chrono>
#include <future>

#include <tinycoro/tinycoro_all.h>

struct SleepAwaiterTest : testing::Test
{
    tinycoro::Scheduler scheduler{4};
};

/*tinycoro::Task<void> SleepTask(auto duration)
{ 
    co_await tinycoro::SleepFor(clock, duration);
};

tinycoro::Task<void> SleepTaskCustom(auto duration)
{
    auto stopToken = co_await tinycoro::StopTokenAwaiter{};
    co_await tinycoro::SleepFor(clock, duration, stopToken);
};*/

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest)
{
    tinycoro::SoftClock clock;

    using namespace std::chrono_literals;

    auto timeout = 200ms;

    auto start  = std::chrono::system_clock::now();
    auto future = scheduler.Enqueue(tinycoro::SleepFor(clock, timeout));

    EXPECT_NO_THROW(future.get());

    EXPECT_TRUE(start + timeout <= std::chrono::system_clock::now());
}

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest_interrupt)
{
    tinycoro::SoftClock clock;
    using namespace std::chrono_literals;

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
    tinycoro::SoftClock clock;
    using namespace std::chrono_literals;

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
    tinycoro::SoftClock clock;
    using namespace std::chrono_literals;

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
    using namespace std::chrono_literals;

    tinycoro::SoftClock clock;
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