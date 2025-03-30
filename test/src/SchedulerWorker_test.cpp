#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/tinycoro_all.h>

TEST(SchedulerWorkerTest, SchedulerWorkerTest_PushTask)
{
    std::stop_source                         ss;
    tinycoro::detail::AtomicQueue<size_t, 2> queue;

    EXPECT_TRUE(tinycoro::detail::helper::PushTask(1, queue, ss));

    ss.request_stop();
    EXPECT_FALSE(tinycoro::detail::helper::PushTask(2, queue, ss));
}

struct SchedubableMock
{
    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(tinycoro::ETaskResumeState, ResumeState, ());
    MOCK_METHOD(void, SetPauseHandler, (tinycoro::PauseHandlerCallbackT));
};

struct Schedubable : tinycoro::detail::ISchedulableBridged
{
    void                       Resume() override { mock.Resume(); };
    tinycoro::ETaskResumeState ResumeState() override { return mock.ResumeState(); };
    void                       SetPauseHandler(tinycoro::PauseHandlerCallbackT cb) override { mock.SetPauseHandler(cb); };

    SchedubableMock mock;
};

TEST(SchedulerWorkerTest, SchedulerWorkerTest_task_execution)
{
    std::latch       latch{1};
    std::stop_source ss;

    std::unique_ptr<Schedubable> task = std::make_unique<Schedubable>();

    EXPECT_CALL(task->mock, Resume).WillOnce([&] { latch.count_down(); });
    EXPECT_CALL(task->mock, SetPauseHandler);
    EXPECT_CALL(task->mock, ResumeState).WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

    tinycoro::detail::AtomicQueue<std::unique_ptr<tinycoro::detail::ISchedulableBridged>, 128> queue;

    tinycoro::detail::SchedulerWorker worker{queue, ss.get_token()};

    queue.try_push(std::move(task));

    latch.wait();
    queue.try_push(tinycoro::detail::helper::SCHEDULER_STOP_EVENT);
}

struct SchedulerWorkerTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SchedulerWorkerTest, SchedulerWorkerTest, testing::Values(1, 10, 100));

TEST_P(SchedulerWorkerTest, SchedulerWorkerTest_task_suspend)
{
    for (size_t i = 0; i < GetParam(); ++i)
    {
        std::latch       latch{2};
        std::stop_source ss;

        std::unique_ptr<Schedubable> task = std::make_unique<Schedubable>();

        EXPECT_CALL(task->mock, Resume).WillRepeatedly([&] { latch.count_down(); });
        EXPECT_CALL(task->mock, SetPauseHandler).Times(2);
        EXPECT_CALL(task->mock, ResumeState)
            .WillOnce(testing::Return(tinycoro::ETaskResumeState::SUSPENDED))
            .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

        tinycoro::detail::AtomicQueue<std::unique_ptr<tinycoro::detail::ISchedulableBridged>, 128> queue;

        tinycoro::detail::SchedulerWorker worker{queue, ss.get_token()};

        queue.try_push(std::move(task));

        latch.wait();
        queue.try_push(tinycoro::detail::helper::SCHEDULER_STOP_EVENT);
    }
}