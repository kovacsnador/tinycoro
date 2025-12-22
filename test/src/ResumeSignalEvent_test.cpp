#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include <tinycoro/ResumeSignalEvent.hpp>

TEST(ResumeSignalEventTest, ResumeSignalEventTest_basic)
{
    tinycoro::detail::ResumeSignalEvent event;

    uint32_t flag{};

    tinycoro::ResumeCallback_t cb = [&]([[maybe_unused]] auto policy) { flag++; };

    event.Set(cb);

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

    tinycoro::detail::ResumeSignalEvent event;

    // no sync is necessary
    uint32_t flag{};
    uint32_t notifyCount{};

    event.Set([&]([[maybe_unused]] auto policy) { flag++; });

    auto work = [&] { 
        for (size_t i = 0; i < count; ++i)
        {
            if(event.Notify())
                notifyCount++;
        }
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
    EXPECT_EQ(notifyCount, 1);
}