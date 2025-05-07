#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

#include "mock/TaskMock.hpp"
#include "mock/CoroutineHandleMock.h"

struct SchedulerFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SchedulerFunctionalTest, SchedulerFunctionalTest, testing::Values(5, 10, 100, 1000, 10000, 100, 100, 100, 100, 100));


TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_destroy)
{
    const auto count = GetParam();

    std::stop_source ss;
    tinycoro::SoftClock clock;

    {
        tinycoro::CustomScheduler<128> scheduler;

        ss = scheduler.GetStopSource();

        for(size_t i = 0; i < count; ++i)
        {
            std::ignore = scheduler.Enqueue(tinycoro::SleepFor(clock, 70ms, ss.get_token()));
        }

        // and we leave this to die.
        //
        // scheduler destructor need to request
        // a stop for the worker threads, and
        // they will stop as soon as possible.
        // The tasks they left in the queues
        // need to be destroyed properly
        //
        // This test is intended to be checked with sanitizers
    }
}

TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_full_queue_cache_task)
{
    const auto count = GetParam();

    tinycoro::CustomScheduler<2> scheduler{};

    std::atomic<size_t> cc{};

    // iterative task
    auto task = [&](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{};
        }
        cc++;
    };

    const auto duration = 10s;

    std::vector<tinycoro::Task<void>> tasks;
    tasks.reserve(count);
    for([[maybe_unused]] auto _ : std::views::iota(3u, count))
    {
        tasks.emplace_back(task(duration));
    }
    tasks.push_back(task(10ms));

    auto start = std::chrono::system_clock::now();
    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_TRUE(std::chrono::system_clock::now() - start < duration);
    EXPECT_EQ(cc, 1);
}