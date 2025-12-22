#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include <tinycoro/ResumeSignalEvent.hpp>

TEST(ResumeSignalEventTest, ResumeSignalEventTest_basic)
{
    tinycoro::detail::ResumeSignalEvent event;

    uint32_t flag{};

    tinycoro::ResumeCallback_t cb = [&](auto policy) { flag++; };

    event.Set(cb);

    EXPECT_EQ(flag, 0);

    event.Notify();

    EXPECT_EQ(flag, 1);

    event.Notify();
    event.Notify();

    EXPECT_EQ(flag, 1);
}

struct ResumeSignalEventTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(ResumeSignalEventTest, ResumeSignalEventTest, testing::Values(1, 10, 100, 1000));

TEST_P(ResumeSignalEventTest, ResumeSignalEventTest_multithreaded)
{
    auto count = GetParam();

    tinycoro::detail::ResumeSignalEvent event;
    uint32_t flag{};

    event.Set([&](auto policy) { flag++; });

    auto work = [&] { 
        for (size_t i = 0; i < count; ++i)
            event.Notify();
    };

    {
        std::vector<std::jthread> threads;
        for (size_t i = 0; i < 8; ++i)
        {
            threads.emplace_back(work);
        }

        // join all the threads
    }

    EXPECT_EQ(flag, 1);
}