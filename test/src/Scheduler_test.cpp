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

    tinycoro::CustomScheduler<2> scheduler;

    std::atomic<size_t> cc{};

    // iterative task
    auto task = [&](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::this_coro::yield_cancellable();
        }
        cc++;
    };

    const auto duration = 1s;

    std::vector<tinycoro::Task<void>> tasks;
    tasks.reserve(count);
    tasks.push_back(task(10ms));
    for([[maybe_unused]] auto _ : std::views::iota(3u, count))
    {
        tasks.emplace_back(task(duration));
    }

    auto start = std::chrono::system_clock::now();
    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_TRUE(std::chrono::system_clock::now() - start < (duration + 1s));
    EXPECT_EQ(cc, 1);

    // this test is intendted to used
    // with sanitizers.
}

TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_stop_source)
{
    auto count = GetParam();

    tinycoro::Scheduler scheduler;

    auto simpleTask = []() -> tinycoro::Task<int32_t>{
        co_return 42;
    };

    auto task = [&]() -> tinycoro::Task<> {
        // close the scheduler
        scheduler.GetStopSource().request_stop();

        for(size_t i=0; i < count; ++i)
        {
            auto res = co_await tinycoro::AllOfAwait(scheduler, simpleTask());
            EXPECT_FALSE(res.has_value());
        }
    };

    // create detached tasks
    for(size_t i = 0; i < count; ++i)
        tinycoro::AllOf(scheduler, tinycoro::Detach{task()});

    // this test is for the sanitizers
}