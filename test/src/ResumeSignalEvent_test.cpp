#include <gtest/gtest.h>

#include <thread>
#include <vector>
#include <ranges>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/ResumeSignalEvent.hpp>

TEST(ResumeSignalEventTest, ResumeSignalEventTest_basic)
{
    tinycoro::detail::ResumeSignalEvent event;

    uint32_t flag{};

    event.Set(tinycoro::test::ResumeCallbackTracer(flag));

    EXPECT_EQ(flag, 0);

    EXPECT_TRUE(event.Notify());

    EXPECT_EQ(flag, 1);

    EXPECT_FALSE(event.Notify());
    EXPECT_FALSE(event.Notify());

    EXPECT_EQ(flag, 1);
}

struct ResumeSignalEventTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(ResumeSignalEventTest, ResumeSignalEventTest, testing::Values(1, 10, 100, 1000));

TEST_P(ResumeSignalEventTest, ResumeSignalEventTest_multithreaded)
{
    auto count = GetParam();

    for (size_t i = 0; i < count; i++)
    {
        tinycoro::detail::ResumeSignalEvent event;

        // No extra synchronization is necessary here:
        // `ResumeSignalEvent` is one-shot so only a single thread will observe `Notify()` == true
        // and perform the increments. The main thread reads the counters after joining the threads.
        uint32_t flag{};
        uint32_t notifyCount{};

        event.Set(tinycoro::test::ResumeCallbackTracer(flag));

        auto work = [&] {
            for (size_t i = 0; i < count; ++i)
            {
                if (event.Notify())
                {
                    notifyCount++;
                    std::atomic_thread_fence(std::memory_order::release);
                }
            }
        };

        {
            std::vector<std::jthread> threads;
            for ([[maybe_unused]] auto _ : std::views::iota(0u, 8u))
            {
                threads.emplace_back(work);
            }
        }
        // join all the threads

        EXPECT_EQ(flag, 1);
        EXPECT_EQ(notifyCount, 1);
    }
}