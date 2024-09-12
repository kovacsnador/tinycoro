#include <gtest/gtest.h>

#include <tinycoro/Scheduler.hpp>
#include <tinycoro/Task.hpp>

#include "mock/TaskMock.hpp"

struct SchedulerTest : public testing::Test
{
    tinycoro::CoroScheduler scheduler{4};
};

TEST_F(SchedulerTest, SchedulerTest_done)
{
    tinycoro::test::TaskMock<int32_t> task;

    using enum tinycoro::ETaskResumeState; 

    EXPECT_CALL(*task.mock, Resume()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(*task.mock, SetPauseHandler).Times(1);
    EXPECT_CALL(*task.mock, await_resume).Times(1).WillOnce(testing::Return(42));

    auto future = scheduler.Enqueue(std::move(task));
    auto val = future.get();

    EXPECT_EQ(val, 42);
}

TEST_F(SchedulerTest, SchedulerTest_suspended)
{
    tinycoro::test::TaskMock<int32_t> task;

    using enum tinycoro::ETaskResumeState; 

    EXPECT_CALL(*task.mock, Resume()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(*task.mock, SetPauseHandler).Times(1);
    EXPECT_CALL(*task.mock, await_resume).Times(1).WillOnce(testing::Return(42));

    auto future = scheduler.Enqueue(std::move(task));
    auto val = future.get();

    EXPECT_EQ(val, 42);
}

TEST_F(SchedulerTest, SchedulerTest_paused)
{
    tinycoro::test::TaskMock<int32_t> task;

    using enum tinycoro::ETaskResumeState;

    auto& mock = *task.mock;

    EXPECT_CALL(mock, Resume()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(PAUSED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock, SetPauseHandler).Times(1);
    EXPECT_CALL(mock, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock, IsPaused).Times(1).WillOnce(testing::Return(false));

    auto future = scheduler.Enqueue(std::move(task));

    // trigger callback to unpause task
    mock.pauseCallback();

    auto val = future.get();

    EXPECT_EQ(val, 42);
}

TEST_F(SchedulerTest, SchedulerTest_multiTasks)
{
    tinycoro::test::TaskMock<int32_t> task1;
    tinycoro::test::TaskMock<int32_t> task2;
    tinycoro::test::TaskMock<void> task3;

    using enum tinycoro::ETaskResumeState;

    auto& mock1 = *task1.mock;

    EXPECT_CALL(mock1, Resume()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(PAUSED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock1, SetPauseHandler).Times(1);
    EXPECT_CALL(mock1, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock1, IsPaused).Times(1).WillOnce(testing::Return(false));

    auto& mock2 = *task2.mock;

    EXPECT_CALL(mock2, Resume()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock2, SetPauseHandler).Times(1);
    EXPECT_CALL(mock2, await_resume).Times(1).WillOnce(testing::Return(41));
    EXPECT_CALL(mock2, IsPaused).Times(0);

    auto& mock3 = *task3.mock;

    EXPECT_CALL(mock3, Resume()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock3, SetPauseHandler).Times(1);
    EXPECT_CALL(mock3, await_resume).Times(0);
    EXPECT_CALL(mock3, IsPaused).Times(0);

    auto futures = scheduler.EnqueueTasks(std::move(task1), std::move(task2), std::move(task3));

    // trigger callback to unpause task
    mock1.pauseCallback();

    EXPECT_EQ(std::get<0>(futures).get(), 42);
    EXPECT_EQ(std::get<1>(futures).get(), 41);
    std::get<2>(futures).get();
}

TEST_F(SchedulerTest, SchedulerTest_multiTasks_dynmic)
{
    tinycoro::test::TaskMock<int32_t> task1;
    tinycoro::test::TaskMock<int32_t> task2;
    tinycoro::test::TaskMock<int32_t> task3;

    using enum tinycoro::ETaskResumeState;

    auto& mock1 = *task1.mock;

    EXPECT_CALL(mock1, Resume()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(PAUSED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock1, SetPauseHandler).Times(1);
    EXPECT_CALL(mock1, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock1, IsPaused).Times(1).WillOnce(testing::Return(false));

    auto& mock2 = *task2.mock;

    EXPECT_CALL(mock2, Resume()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock2, SetPauseHandler).Times(1);
    EXPECT_CALL(mock2, await_resume).Times(1).WillOnce(testing::Return(41));
    EXPECT_CALL(mock2, IsPaused).Times(0);

    auto& mock3 = *task3.mock;

    EXPECT_CALL(mock3, Resume()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock3, SetPauseHandler).Times(1);
    EXPECT_CALL(mock3, await_resume).Times(1).WillOnce(testing::Return(40));
    EXPECT_CALL(mock3, IsPaused).Times(0);

    std::vector<decltype(task1)> tasks{std::move(task1), std::move(task2), std::move(task3)};

    auto futures = scheduler.EnqueueTasks(std::move(tasks));

    // trigger callback to unpause task
    mock1.pauseCallback();

    EXPECT_EQ(futures[0].get(), 42);
    EXPECT_EQ(futures[1].get(), 41);
    EXPECT_EQ(futures[2].get(), 40);
}

TEST_F(SchedulerTest, SchedulerTest_multiTasks_Wait)
{
    tinycoro::test::TaskMock<int32_t> task1;
    tinycoro::test::TaskMock<int32_t> task2;
    tinycoro::test::TaskMock<void> task3;

    using enum tinycoro::ETaskResumeState;

    auto& mock1 = *task1.mock;

    EXPECT_CALL(mock1, Resume()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(PAUSED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock1, SetPauseHandler).Times(1);
    EXPECT_CALL(mock1, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock1, IsPaused).Times(1).WillOnce(testing::Return(false));

    auto& mock2 = *task2.mock;

    EXPECT_CALL(mock2, Resume()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock2, SetPauseHandler).Times(1);
    EXPECT_CALL(mock2, await_resume).Times(1).WillOnce(testing::Return(41));
    EXPECT_CALL(mock2, IsPaused).Times(0);

    auto& mock3 = *task3.mock;

    EXPECT_CALL(mock3, Resume()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock3, SetPauseHandler).Times(1);
    EXPECT_CALL(mock3, await_resume).Times(0);
    EXPECT_CALL(mock3, IsPaused).Times(0);

    auto futures = scheduler.EnqueueTasks(std::move(task1), std::move(task2), std::move(task3));

    // trigger callback to unpause task
    mock1.pauseCallback();

    scheduler.Wait();
}