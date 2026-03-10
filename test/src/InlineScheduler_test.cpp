#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

TEST(InlineSchedulerTest, InlineSchedulerTest_constructor)
{
    tinycoro::InlineScheduler scheduler;

    tinycoro::TaskGroup<int32_t> group;

    auto task = []()->tinycoro::Task<int32_t> { co_return 42; };

    EXPECT_TRUE(group.Spawn(scheduler, task()));

    scheduler.Run();

    auto val = group.TryNext();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);

    EXPECT_TRUE(group.Spawn(scheduler, task()));
    scheduler.Run();

    val = group.TryNext();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
}

TEST(InlineSchedulerTest, InlineSchedulerTest_co_await)
{
    tinycoro::InlineScheduler scheduler;

    tinycoro::TaskGroup<void> group;

    auto task = [&scheduler]() mutable -> tinycoro::Task<void> { 
        
        auto task2 = [](int32_t v) -> tinycoro::Task<int32_t> { co_return v; };

        auto [res1, res2, res3] = co_await AllOfAwait(scheduler, task2(1), task2(2), task2(3));
        EXPECT_EQ(*res1, 1);
        EXPECT_EQ(*res2, 2);
        EXPECT_EQ(*res3, 3);
    };

    group.Spawn(scheduler, task());

    scheduler.Run();
}

TEST(InlineSchedulerTest, InlineSchedulerTest_enqueue_tuple)
{
    tinycoro::InlineScheduler scheduler;

    auto task = [](int32_t v) -> tinycoro::Task<int32_t> { co_return v; };

    auto [fut1, fut2, fut3] = scheduler.Enqueue(task(1), task(2), task(3));
    scheduler.Run();

    auto val1 = fut1.get();
    auto val2 = fut2.get();
    auto val3 = fut3.get();

    ASSERT_TRUE(val1.has_value());
    ASSERT_TRUE(val2.has_value());
    ASSERT_TRUE(val3.has_value());
    EXPECT_EQ(*val1, 1);
    EXPECT_EQ(*val2, 2);
    EXPECT_EQ(*val3, 3);
}

TEST(InlineSchedulerTest, InlineSchedulerTest_enqueue_container_with_invalid_task)
{
    tinycoro::InlineScheduler scheduler;

    auto task = [](int32_t v) -> tinycoro::Task<int32_t> { co_return v; };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.emplace_back(task(1));
    tasks.emplace_back(); // default-constructed task has no coroutine handle
    tasks.emplace_back(task(3));

    auto futures = scheduler.Enqueue(std::move(tasks));
    scheduler.Run();

    ASSERT_EQ(futures.size(), 3u);

    auto val1 = futures[0].get();
    auto val2 = futures[1].get();
    auto val3 = futures[2].get();

    ASSERT_TRUE(val1.has_value());
    EXPECT_FALSE(val2.has_value());
    ASSERT_TRUE(val3.has_value());
    EXPECT_EQ(*val1, 1);
    EXPECT_EQ(*val3, 3);
}

TEST(InlineSchedulerTest, InlineSchedulerTest_exception_forwarded_to_future)
{
    tinycoro::InlineScheduler scheduler;

    auto okTask   = []() -> tinycoro::Task<int32_t> { co_return 42; };
    auto failTask = []() -> tinycoro::Task<int32_t> {
        throw std::runtime_error{"InlineSchedulerTest_exception"};
        co_return 0;
    };

    auto okFuture   = scheduler.Enqueue(okTask());
    auto failFuture = scheduler.Enqueue(failTask());

    scheduler.Run();

    auto val = okFuture.get();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);

    EXPECT_THROW((void)failFuture.get(), std::runtime_error);
}

TEST(InlineSchedulerTest, InlineSchedulerTest_run_without_work_returns_immediately)
{
    tinycoro::InlineScheduler scheduler;

    auto runFuture = std::async(std::launch::async, [&scheduler] {
        scheduler.Run();
        return true;
    });

    EXPECT_EQ(runFuture.wait_for(200ms), std::future_status::ready);
    EXPECT_TRUE(runFuture.get());
}

TEST(InlineSchedulerTest, InlineSchedulerTest_work_guard_keeps_scheduler_alive)
{
    tinycoro::InlineScheduler scheduler;

    auto workGuard = tinycoro::MakeWorkGuard(scheduler);

    auto runFuture = std::async(std::launch::async, [&scheduler] {
        scheduler.Run();
        return true;
    });

    // While a WorkGuard exists, Run() must stay alive.
    EXPECT_EQ(runFuture.wait_for(100ms), std::future_status::timeout);

    workGuard.Unlock();

    EXPECT_EQ(runFuture.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(runFuture.get());
}

TEST(InlineSchedulerTest, InlineSchedulerTest_work_guard_with_cross_thread_enqueue)
{
    tinycoro::InlineScheduler scheduler;

    auto workGuard = tinycoro::MakeWorkGuard(scheduler);

    std::atomic<int32_t> sum{};

    auto addTask = [&sum](int32_t v) -> tinycoro::Task<> {
        sum.fetch_add(v, std::memory_order_relaxed);
        co_return;
    };

    auto runFuture = std::async(std::launch::async, [&scheduler] {
        scheduler.Run();
        return true;
    });

    auto fut1 = scheduler.Enqueue(addTask(1));
    auto fut2 = scheduler.Enqueue(addTask(2));
    auto fut3 = scheduler.Enqueue(addTask(3));

    workGuard.Unlock();

    ASSERT_EQ(runFuture.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(runFuture.get());

    EXPECT_TRUE(fut1.get().has_value());
    EXPECT_TRUE(fut2.get().has_value());
    EXPECT_TRUE(fut3.get().has_value());
    EXPECT_EQ(sum.load(std::memory_order_relaxed), 6);
}

TEST(InlineSchedulerTest, InlineSchedulerTest_work_guard)
{
    tinycoro::InlineScheduler scheduler;

    auto workGuard = tinycoro::MakeWorkGuard(scheduler);

    auto fut = std::async(std::launch::async, [wg = std::move(workGuard), &scheduler] () mutable {
        
        auto workGuard = std::move(wg);
        
        tinycoro::TaskGroup<int32_t> group;

        auto task = [](int32_t v)->tinycoro::Task<int32_t> { co_return v; };

        group.Spawn(scheduler, task(1));
        group.Spawn(scheduler, task(2));
        group.Spawn(scheduler, task(3));

        tinycoro::Join(group);

        auto res1 = group.TryNext();
        auto res2 = group.TryNext();
        auto res3 = group.TryNext();

        EXPECT_EQ(*res1, 1);
        EXPECT_EQ(*res2, 2);
        EXPECT_EQ(*res3, 3);
    });

    scheduler.Run();

    fut.get();
}
