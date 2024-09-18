#include <gtest/gtest.h>

#include <chrono>
#include <future>

#include <tinycoro/tinycoro_all.h>

struct SleepAwaiterTest : testing::Test
{
    tinycoro::CoroScheduler scheduler{4};
};

TEST_F(SleepAwaiterTest, SimpleSleepAwaiterTest)
{
    using namespace std::chrono_literals;

    auto timeout = 200ms;

    auto sleepTask = [timeout]() -> tinycoro::Task<void> { co_await tinycoro::Sleep(timeout); };

    auto start  = std::chrono::system_clock::now();
    auto future = scheduler.Enqueue(sleepTask());

    EXPECT_NO_THROW(future.get());

    EXPECT_TRUE(start + timeout <= std::chrono::system_clock::now());
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