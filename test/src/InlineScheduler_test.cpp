#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

#include <future>

TEST(InlineSchedulerTest, InlineSchedulerTest_constructor)
{
    tinycoro::InlineScheduler scheduler;

    tinycoro::TaskGroup<int32_t> group;

    auto task = []()->tinycoro::Task<int32_t> { co_return 42; };

    group.Spawn(scheduler, task());

    scheduler.Run();

    auto val = group.TryNext();
    EXPECT_EQ(*val, 42);

    group.Spawn(scheduler, task());
    scheduler.Run();

    val = group.TryNext();
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