#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

#include <ranges>

#include "mock/TaskMock.hpp"
#include "mock/CoroutineHandleMock.h"

TEST(ParallelSchedulerTest, constructor_throw)
{
    EXPECT_THROW(tinycoro::Scheduler{0}, tinycoro::SchedulerException);
}

struct SchedulerFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SchedulerFunctionalTest, SchedulerFunctionalTest, testing::Values(5, 10, 100, 1000, 10000, 30000));

TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_destroy)
{
    const auto count = GetParam();

    std::stop_source    ss;
    tinycoro::SoftClock clock;

    {
        tinycoro::CustomScheduler<128> scheduler;

        ss = scheduler.StopSource();

        for (size_t i = 0; i < count; ++i)
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
    for ([[maybe_unused]] auto _ : std::views::iota(3u, count))
    {
        tasks.emplace_back(task(duration));
    }

    auto start = std::chrono::system_clock::now();
    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_TRUE(std::chrono::system_clock::now() - start < (duration + 1s));

    // this test is intendted to used
    // with sanitizers.
}

TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_stop_source)
{
    auto count = GetParam();

    tinycoro::Scheduler scheduler;

    auto simpleTask = []() -> tinycoro::Task<int32_t> { co_return 42; };

    auto task = [&]() -> tinycoro::Task<> {
        // close the scheduler
        scheduler.StopSource().request_stop();

        for (size_t i = 0; i < count; ++i)
        {
            auto res = co_await tinycoro::AllOfAwait(scheduler, simpleTask());
            EXPECT_FALSE(res.has_value());
        }
    };

    // create detached tasks
    for (size_t i = 0; i < count; ++i)
        tinycoro::AllOf(scheduler, tinycoro::Detach{task()});

    // this test is for the sanitizers
}

TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_external_token)
{
    auto count = GetParam();

    std::stop_source ss;

    tinycoro::Scheduler scheduler{ss.get_token()};

    auto simpleTask = []() -> tinycoro::Task<int32_t> { co_return 42; };

    auto task = [&]() -> tinycoro::Task<> {
        // close the scheduler
        ss.request_stop();

        // make sure stop is triggered
        while (scheduler.StopToken().stop_requested() == false)
        {
        }

        for (size_t i = 0; i < count; ++i)
        {
            auto res = co_await tinycoro::AllOfAwait(scheduler, simpleTask());
            EXPECT_FALSE(res.has_value());
        }
    };

    // create detached tasks
    for (size_t i = 0; i < count; ++i)
        tinycoro::AllOf(scheduler, tinycoro::Detach{task()});

    // this test is for the sanitizers
}

TEST_P(SchedulerFunctionalTest, SchedulerFunctionalTest_all_must_finish)
{
    std::atomic<size_t> finished{};

    auto count = GetParam();

    {
        std::vector<tinycoro::AutoEvent> events1(count);
        std::vector<tinycoro::AutoEvent> events2(count);
        std::vector<tinycoro::AutoEvent> events3(count);

        tinycoro::Scheduler scheduler{};

        std::vector<tinycoro::Task<>> tasks;

        auto producer = [&](size_t i) -> tinycoro::Task<> {
            events1[i].Set();
            co_await events2[i];
            events3[i].Set();
            finished.fetch_add(1, std::memory_order::relaxed);
        };

        auto consumer = [&](size_t i) -> tinycoro::Task<> {
            co_await events1[i];
            events2[i].Set();
            co_await events3[i];
            finished.fetch_add(1, std::memory_order::relaxed);
        };

        for (size_t i = 0; i < count; ++i)
        {
            tasks.push_back(producer(i));
            tasks.push_back(consumer(i));
        }

        std::ignore = scheduler.Enqueue(std::move(tasks));
        scheduler.StopSource().request_stop();
    }

    EXPECT_EQ(finished, count * 2);

    // this test is for the sanitizers
}