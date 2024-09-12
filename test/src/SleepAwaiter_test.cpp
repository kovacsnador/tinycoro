#include <gtest/gtest.h>

#include <chrono>

#include <tinycoro/SleepAwaiter.hpp>
#include <tinycoro/Scheduler.hpp>

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

using namespace std::chrono_literals;


TEST(SleepAwaiterTest, SleepAwaiterTest)
{
    auto timeout = 200ms;

    auto task = [timeout]() -> tinycoro::Task<void> {
        co_await tinycoro::Sleep(timeout);
    };

    tinycoro::CoroScheduler scheduler{4};

    auto start = std::chrono::system_clock::now();
    auto future = scheduler.Enqueue(task());

    EXPECT_NO_THROW(future.get());

    EXPECT_TRUE(start + timeout <= std::chrono::system_clock::now());
}