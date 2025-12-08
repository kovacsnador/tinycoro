#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/tinycoro_all.h>

TEST(SchedulerWorkerTest, SchedulerWorkerTest_PushTask)
{
    std::stop_source                         ss;
    tinycoro::detail::AtomicQueue<size_t, 2> queue;
    tinycoro::detail::Dispatcher dispatcher{queue};

    EXPECT_TRUE(tinycoro::detail::helper::PushTask(1, dispatcher, ss));

    ss.request_stop();
    EXPECT_FALSE(tinycoro::detail::helper::PushTask(2, dispatcher, ss));
}

TEST(RequestStopForQueueTest, RequestStopForQueue_fullQueue)
{
    int32_t val{42};

    tinycoro::detail::AtomicQueue<int32_t*, 2> queue;

    tinycoro::detail::Dispatcher dispatcher{queue};

    EXPECT_TRUE(dispatcher.try_push(&val));
    EXPECT_TRUE(dispatcher.try_push(&val));

    EXPECT_TRUE(dispatcher.full());

    tinycoro::detail::helper::RequestStopForQueue(dispatcher);

    int32_t* ptr;
    EXPECT_TRUE(dispatcher.try_pop(ptr));
    EXPECT_EQ(*ptr, val);

    EXPECT_TRUE(dispatcher.try_pop(ptr));
    EXPECT_EQ(ptr, nullptr);
}

struct AtomicQueueMock
{
    using value_type = int32_t*;

    MOCK_METHOD(bool, try_pop, (int32_t*));
    MOCK_METHOD(bool, try_push, (int32_t*));
    MOCK_METHOD(bool, full, ());
};

TEST(RequestStopForQueueTest, RequestStopForQueue_mockQueue)
{
    AtomicQueueMock mock;

    EXPECT_CALL(mock, full).WillOnce(testing::Return(true));

    EXPECT_CALL(mock, try_pop).WillOnce(testing::Return(true)).WillOnce(testing::Return(true));

    EXPECT_CALL(mock, try_push).WillOnce(testing::Return(false)).WillOnce(testing::Return(true));

    tinycoro::detail::helper::RequestStopForQueue(mock);
}

struct SchedubableMock
{
    MOCK_METHOD(tinycoro::ETaskResumeState, Resume, ());

    MOCK_METHOD(std::atomic<tinycoro::EPauseState>&, PauseState, ());

    MOCK_METHOD(void, SetPauseHandler, (tinycoro::PauseHandlerCallbackT));
};

struct Schedubable : tinycoro::detail::DoubleLinkable<Schedubable>
{
    tinycoro::ETaskResumeState Resume() { return mock.Resume(); };

    void SetPauseHandler(tinycoro::PauseHandlerCallbackT cb) { mock.SetPauseHandler(cb); };

    auto& PauseState() { return mock.PauseState(); }

    SchedubableMock mock;

    std::atomic<tinycoro::EPauseState> pauseState{tinycoro::EPauseState::IDLE};
};

TEST(SchedulerWorkerTest, SchedulerWorkerTest_task_execution)
{
    std::latch       latch{1};
    std::stop_source ss;

    std::unique_ptr<Schedubable> task{new Schedubable};

    EXPECT_CALL(task->mock, Resume).WillOnce([&] {
        latch.count_down();
        return tinycoro::ETaskResumeState::DONE;
    });
    EXPECT_CALL(task->mock, SetPauseHandler);

    tinycoro::detail::AtomicQueue<std::unique_ptr<Schedubable>, 128> queue;
    tinycoro::detail::Dispatcher dispatcher{queue};

    tinycoro::detail::SchedulerWorker worker{dispatcher, ss.get_token()};

    EXPECT_TRUE(dispatcher.try_push(std::move(task)));

    latch.wait();
    EXPECT_TRUE(dispatcher.try_push(tinycoro::detail::helper::SCHEDULER_STOP_EVENT));
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

        std::unique_ptr<Schedubable> task{new Schedubable};

        EXPECT_CALL(task->mock, Resume)
            .WillOnce([&] {
                latch.count_down();
                return tinycoro::ETaskResumeState::SUSPENDED;
            })
            .WillOnce([&] {
                latch.count_down();
                return tinycoro::ETaskResumeState::DONE;
            });
        EXPECT_CALL(task->mock, SetPauseHandler).Times(2);

        tinycoro::detail::AtomicQueue<std::unique_ptr<Schedubable>, 128> queue;
        tinycoro::detail::Dispatcher dispatcher{queue};


        tinycoro::detail::SchedulerWorker worker{dispatcher, ss.get_token()};

        EXPECT_TRUE(dispatcher.try_push(std::move(task)));

        latch.wait();
        EXPECT_TRUE(dispatcher.try_push(tinycoro::detail::helper::SCHEDULER_STOP_EVENT));
    }
}