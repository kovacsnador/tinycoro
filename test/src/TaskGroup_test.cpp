#include <gtest/gtest.h>

#include <set>

#include "tinycoro/tinycoro_all.h"

tinycoro::Task<> VoidTask()
{
    co_return;
}

tinycoro::Task<int32_t> IntTask(int32_t val = 42)
{
    co_return val;
}

TEST(TaskGroupTest, TaskGroupTest_one_task)
{
    tinycoro::TaskGroup<void> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, VoidTask());

    tinycoro::Join(taskGroup);
}

TEST(TaskGroupTest, TaskGroupTest_multiple_task)
{
    tinycoro::TaskGroup<void> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, VoidTask());
    taskGroup.Spawn(scheduler, VoidTask());
    taskGroup.Spawn(scheduler, VoidTask());

    tinycoro::Join(taskGroup);
}

TEST(TaskGroupTest, TaskGroupTest_multiple_task_implicit_join)
{
    {
        tinycoro::TaskGroup<void> taskGroup;

        tinycoro::Scheduler scheduler;
        taskGroup.Spawn(scheduler, VoidTask());
        taskGroup.Spawn(scheduler, VoidTask());
        taskGroup.Spawn(scheduler, VoidTask());
    }
}

TEST(TaskGroupTest, TaskGroupTest_multiple_task_try_next_int)
{
    tinycoro::TaskGroup<int32_t> taskGroup;

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    auto task = [&]() -> tinycoro::Task<> {

        tinycoro::Latch latch{3};

        auto intTask = [&](int32_t v)->tinycoro::Task<int32_t>
        {
            auto onExit = tinycoro::Finally([&]{ latch.CountDown(); });
            co_return v;
        };

        taskGroup.Spawn(scheduler, intTask(1));
        taskGroup.Spawn(scheduler, intTask(2));
        taskGroup.Spawn(scheduler, intTask(3));

        // wait for the latch
        co_await latch;

        // safe sleep
        co_await tinycoro::SleepFor(clock, 100ms);

        std::set set{1, 2, 3};

        while (auto res = taskGroup.TryNext())
        {
            EXPECT_TRUE(set.erase(*res));
        }

        EXPECT_TRUE(set.empty());
    };

    tinycoro::AllOf(task());
}

TEST(TaskGroupTest, TaskGroupTest_multiple_task_try_next_void)
{
    tinycoro::TaskGroup<void> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, VoidTask());
    taskGroup.Spawn(scheduler, VoidTask());
    taskGroup.Spawn(scheduler, VoidTask());

    tinycoro::Join(taskGroup);

    size_t count = 0;
    while (auto res = taskGroup.TryNext())
    {
        count++;
    }

    EXPECT_EQ(count, 3);
}

TEST(TaskGroupTest, TaskGroupTest_one_task_return)
{
    tinycoro::TaskGroup<int32_t> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, IntTask(42));

    tinycoro::Join(taskGroup);

    auto val = taskGroup.TryNext().value();

    EXPECT_EQ(val, 42);
}

TEST(TaskGroupTest, TaskGroupTest_multiple_task_return)
{
    tinycoro::TaskGroup<int32_t> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, IntTask(1));
    taskGroup.Spawn(scheduler, IntTask(2));
    taskGroup.Spawn(scheduler, IntTask(3));
    taskGroup.Spawn(scheduler, IntTask(4));

    tinycoro::Join(taskGroup);

    std::set set{1,2,3,4};

    while(auto res = taskGroup.TryNext())
    {
        EXPECT_TRUE(set.erase(*res));
    }

    EXPECT_TRUE(set.empty());
}

/*TEST(TaskGroupTest, TaskGroupTest_multiple_task_return_next)
{
    tinycoro::TaskGroup<int32_t> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, IntTask(1));
    taskGroup.Spawn(scheduler, IntTask(2));
    taskGroup.Spawn(scheduler, IntTask(3));
    taskGroup.Spawn(scheduler, IntTask(4));

    auto val = taskGroup.Next();
    EXPECT_EQ(*val, 1);

    val = taskGroup.Next();
    EXPECT_EQ(*val, 2);

    val = taskGroup.Next();
    EXPECT_EQ(*val, 3);

    val = taskGroup.Next();
    EXPECT_EQ(*val, 4);
}*/

TEST(TaskGroupTest, TaskGroupTest_next_await)
{
    tinycoro::TaskGroup<int32_t> taskGroup;

    tinycoro::Scheduler scheduler;
    taskGroup.Spawn(scheduler, IntTask(1));
    taskGroup.Spawn(scheduler, IntTask(2));
    taskGroup.Spawn(scheduler, IntTask(3));
    taskGroup.Spawn(scheduler, IntTask(4));

    auto task = [&taskGroup]() -> tinycoro::Task<> {
        size_t count{0};

        for (;;)
        {
            auto val = co_await taskGroup.Next();

            if (val.has_value() == false)
                co_return;

            if (++count == 4)
                taskGroup.CancelAll();

            EXPECT_TRUE(*val <= 4);
        }
    };

    tinycoro::AllOf(task());
}

TEST(TaskGroupTest, TaskGroupTest_join_await)
{
    tinycoro::Scheduler scheduler;

    auto task = [&]() -> tinycoro::Task<> {
        std::atomic<int32_t> count{};
        auto                 counter = [&]() -> tinycoro::Task<int32_t> { co_return count.fetch_add(1); };

        tinycoro::TaskGroup<int32_t> taskGroup;

        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());

        // wait for all the task.
        co_await taskGroup.Join();

        std::set set{0,1,2,3};

        while(auto res = co_await taskGroup.Next())
        {
            EXPECT_TRUE(set.erase(*res));
        }

        EXPECT_TRUE(set.empty());
    };

    tinycoro::AllOf(task());
}

TEST(TaskGroupTest, TaskGroupTest_join_await_void)
{
    tinycoro::Scheduler scheduler;

    auto task = [&]() -> tinycoro::Task<> {
        std::atomic<int32_t> count{};
        auto                 counter = [&]() -> tinycoro::Task<> { count.fetch_add(1); co_return; };

        tinycoro::TaskGroup<> taskGroup;

        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());

        // wait for all the task.
        co_await taskGroup.Join();

        while(auto res = co_await taskGroup.Next())
        {
            count--;
        }

        EXPECT_EQ(count, 0);
    };

    tinycoro::AllOf(task());
}

TEST(TaskGroupTest, TaskGroupTest_join_await_timeout)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    auto task = [&]() -> tinycoro::Task<> {
        std::atomic<int32_t> count{};

        auto counter = [&](auto time) -> tinycoro::Task<int32_t> {
            co_await tinycoro::SleepFor(clock, time);
            co_return count.fetch_add(1);
        };

        tinycoro::TaskGroup<int32_t> taskGroup;

        taskGroup.Spawn(scheduler, counter(5ms));
        taskGroup.Spawn(scheduler, counter(20ms));
        taskGroup.Spawn(scheduler, counter(50ms));
        taskGroup.Spawn(scheduler, counter(100ms));

        // wait for all the task.
        co_await taskGroup.Join();

        EXPECT_EQ(count, 4);
    };

    tinycoro::AllOf(task());
}

TEST(TaskGroupTest, TaskGroupTest_join_await_cancelled)
{
    tinycoro::Scheduler scheduler;

    auto task = [&]() -> tinycoro::Task<> {
        std::atomic<int32_t> count{};

        tinycoro::AutoEvent event;

        auto counter = [&]() -> tinycoro::TaskNIC<int32_t> {
            co_await tinycoro::Cancellable(event.Wait());
            co_return count++;
        };

        tinycoro::TaskGroup<int32_t> taskGroup;

        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());
        taskGroup.Spawn(scheduler, counter());

        taskGroup.CancelAll();

        // wait for all the task.
        co_await taskGroup.Join();

        while(auto res = co_await taskGroup.Next())
        {
            count++;
        }

        EXPECT_EQ(count, 0);
    };

    tinycoro::AllOf(task());
}

TEST(TaskGroupTest, TaskGroupTest_spawn_after_close_fails)
{
    tinycoro::Scheduler scheduler;

    tinycoro::TaskGroup<int> group;

    group.Close();

    auto task = []() -> tinycoro::Task<int> {
        co_return 1;
    };

    EXPECT_FALSE(group.Spawn(scheduler, task()));
}