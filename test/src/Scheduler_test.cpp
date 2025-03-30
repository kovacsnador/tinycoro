#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

#include "mock/TaskMock.hpp"

static void* dummyAddress = reinterpret_cast<void*>(0xDEADBEEF); 

struct SchedulerTest : public testing::Test
{
    tinycoro::Scheduler scheduler{4};
};

TEST_F(SchedulerTest, SchedulerTest_done)
{
    tinycoro::test::TaskMock<int32_t> task;

    using enum tinycoro::ETaskResumeState; 

    EXPECT_CALL(*task.mock, Resume()).Times(1);
    EXPECT_CALL(*task.mock, IsDone()).WillOnce(testing::Return(true));
    EXPECT_CALL(*task.mock, ResumeState()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(*task.mock, SetPauseHandler).Times(1);
    EXPECT_CALL(*task.mock, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(*task.mock, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));


    auto future = scheduler.Enqueue(std::move(task));
    auto val = future.get();

    EXPECT_EQ(val, 42);
}

TEST_F(SchedulerTest, SchedulerTest_suspended)
{
    tinycoro::test::TaskMock<int32_t> task;

    using enum tinycoro::ETaskResumeState; 

    EXPECT_CALL(*task.mock, Resume()).Times(2);
    EXPECT_CALL(*task.mock, IsDone()).WillOnce(testing::Return(true));
    EXPECT_CALL(*task.mock, ResumeState()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(*task.mock, SetPauseHandler).Times(2);
    EXPECT_CALL(*task.mock, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(*task.mock, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));

    auto future = scheduler.Enqueue(std::move(task));
    auto val = future.get();

    EXPECT_EQ(val, 42);
}

TEST_F(SchedulerTest, SchedulerTest_paused)
{
    tinycoro::test::TaskMock<int32_t> task;

    using enum tinycoro::ETaskResumeState;

    auto& mock = *task.mock;

    EXPECT_CALL(mock, Resume()).Times(3);
    EXPECT_CALL(mock, IsDone()).Times(testing::AnyNumber());
    EXPECT_CALL(mock, ResumeState()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Invoke([&mock] { mock.pauseCallback(); return PAUSED; })).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock, SetPauseHandler).Times(2);
    EXPECT_CALL(mock, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock, IsPaused).Times(0);
    EXPECT_CALL(mock, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));;

    auto future = scheduler.Enqueue(std::move(task));

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

    EXPECT_CALL(mock1, Resume()).Times(3);
    EXPECT_CALL(mock1, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock1, ResumeState()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Invoke([&mock1] { mock1.pauseCallback(); return PAUSED; })).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock1, SetPauseHandler).Times(2);
    EXPECT_CALL(mock1, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock1, IsPaused).Times(0);
    EXPECT_CALL(mock1, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));;

    auto& mock2 = *task2.mock;

    EXPECT_CALL(mock2, Resume()).Times(2);
    EXPECT_CALL(mock2, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock2, ResumeState()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock2, SetPauseHandler).Times(2);
    EXPECT_CALL(mock2, await_resume).Times(1).WillOnce(testing::Return(41));
    EXPECT_CALL(mock2, IsPaused).Times(0);
    EXPECT_CALL(mock2, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));

    auto& mock3 = *task3.mock;

    EXPECT_CALL(mock3, Resume()).Times(1);
    EXPECT_CALL(mock3, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock3, ResumeState()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock3, SetPauseHandler).Times(1);
    EXPECT_CALL(mock3, await_resume).Times(0);
    EXPECT_CALL(mock3, IsPaused).Times(0);
    EXPECT_CALL(mock3, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));

    auto futures = scheduler.Enqueue(std::move(task1), std::move(task2), std::move(task3));

    EXPECT_EQ(std::get<0>(futures).get().value(), 42);
    EXPECT_EQ(std::get<1>(futures).get().value(), 41);
    EXPECT_NO_THROW(std::get<2>(futures).get());
}

TEST_F(SchedulerTest, SchedulerTest_multiTasks_dynmic)
{
    tinycoro::test::TaskMock<int32_t> task1;
    tinycoro::test::TaskMock<int32_t> task2;
    tinycoro::test::TaskMock<int32_t> task3;

    using enum tinycoro::ETaskResumeState;

    auto& mock1 = *task1.mock;

    EXPECT_CALL(mock1, Resume()).Times(3);
    EXPECT_CALL(mock1, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock1, ResumeState()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Invoke([&mock1] { mock1.pauseCallback(); return PAUSED; })).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock1, SetPauseHandler).Times(2);
    EXPECT_CALL(mock1, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock1, IsPaused).Times(0);
    EXPECT_CALL(mock1, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));;

    auto& mock2 = *task2.mock;

    EXPECT_CALL(mock2, Resume()).Times(2);
    EXPECT_CALL(mock2, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock2, ResumeState()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock2, SetPauseHandler).Times(2);
    EXPECT_CALL(mock2, await_resume).Times(1).WillOnce(testing::Return(41));
    EXPECT_CALL(mock2, IsPaused).Times(0);
    EXPECT_CALL(mock2, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));

    auto& mock3 = *task3.mock;

    EXPECT_CALL(mock3, Resume()).Times(1);
    EXPECT_CALL(mock3, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock3, ResumeState()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock3, SetPauseHandler).Times(1);
    EXPECT_CALL(mock3, await_resume).Times(1).WillOnce(testing::Return(40));
    EXPECT_CALL(mock3, IsPaused).Times(0);
    EXPECT_CALL(mock3, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));


    std::vector<decltype(task1)> tasks{std::move(task1), std::move(task2), std::move(task3)};

    auto futures = scheduler.Enqueue(std::move(tasks));

    EXPECT_EQ(futures[0].get().value(), 42);
    EXPECT_EQ(futures[1].get().value(), 41);
    EXPECT_EQ(futures[2].get().value(), 40);
}

TEST_F(SchedulerTest, SchedulerTest_multiTasks_Wait)
{
    tinycoro::test::TaskMock<int32_t> task1;
    tinycoro::test::TaskMock<int32_t> task2;
    tinycoro::test::TaskMock<void> task3;

    using enum tinycoro::ETaskResumeState;

    auto& mock1 = *task1.mock;

    EXPECT_CALL(mock1, Resume()).Times(3);
    EXPECT_CALL(mock1, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock1, ResumeState()).Times(3).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Invoke([&mock1] { mock1.pauseCallback(); return PAUSED; })).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock1, SetPauseHandler).Times(2);
    EXPECT_CALL(mock1, await_resume).Times(1).WillOnce(testing::Return(42));
    EXPECT_CALL(mock1, IsPaused).Times(0);
    EXPECT_CALL(mock1, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));;


    auto& mock2 = *task2.mock;

    EXPECT_CALL(mock2, Resume()).Times(2);
    EXPECT_CALL(mock2, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock2, ResumeState()).Times(2).WillOnce(testing::Return(SUSPENDED)).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock2, SetPauseHandler).Times(2);
    EXPECT_CALL(mock2, await_resume).Times(1).WillOnce(testing::Return(41));
    EXPECT_CALL(mock2, IsPaused).Times(0);
    EXPECT_CALL(mock2, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));


    auto& mock3 = *task3.mock;

    EXPECT_CALL(mock3, Resume());
    EXPECT_CALL(mock3, IsDone()).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock3, ResumeState()).Times(1).WillOnce(testing::Return(DONE));
    EXPECT_CALL(mock3, SetPauseHandler).Times(1);
    EXPECT_CALL(mock3, await_resume).Times(0);
    EXPECT_CALL(mock3, IsPaused).Times(0);
    EXPECT_CALL(mock3, Address).Times(1).WillRepeatedly(testing::Return(dummyAddress));


    auto futures = scheduler.Enqueue(std::move(task1), std::move(task2), std::move(task3));

    auto[t1, t2, t3] = tinycoro::GetAll(futures);
    
    EXPECT_EQ(*t1, 42);
    EXPECT_EQ(*t2, 41);
    EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, decltype(t3)>));
}


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
    tinycoro::AnyOf(scheduler, tasks);

    EXPECT_TRUE(std::chrono::system_clock::now() - start < duration);
    EXPECT_EQ(cc, 1);
}