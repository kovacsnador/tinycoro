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

TEST(RequestStopForQueueTest, RequestStopForQueue_fullQueue)
{
    int32_t val{42};

    tinycoro::detail::AtomicQueue<int32_t*, 2> queue;
    EXPECT_TRUE(queue.try_push(&val));
    EXPECT_TRUE(queue.try_push(&val));

    EXPECT_TRUE(queue.full());

    tinycoro::detail::helper::RequestStopForQueue(queue);

    int32_t* ptr;
    EXPECT_TRUE(queue.try_pop(ptr));
    EXPECT_EQ(*ptr, val);
    
    EXPECT_TRUE(queue.try_pop(ptr));
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
    
    EXPECT_CALL(mock, try_pop)
        .WillOnce(testing::Return(true))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(mock, try_push)
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true));

    tinycoro::detail::helper::RequestStopForQueue(mock);
}

struct SchedubableMock
{
    MOCK_METHOD(tinycoro::ETaskResumeState, Resume, ());

    MOCK_METHOD(void, SetPauseHandler, (tinycoro::PauseHandlerCallbackT));
};

using IScheduler = tinycoro::detail::ISchedulableBridged<tinycoro::DefaultAllocator_t>;

struct Schedubable : IScheduler
{
    Schedubable()
    : IScheduler{alloc, sizeof(*this)}
    {
    }

    tinycoro::ETaskResumeState Resume() override { return mock.Resume(); };
    void                       SetPauseHandler(tinycoro::PauseHandlerCallbackT cb) override { mock.SetPauseHandler(cb); };

    SchedubableMock mock;

    tinycoro::DefaultAllocator_t alloc{};
};

TEST(SchedulerWorkerTest, SchedulerWorkerTest_task_execution)
{
    std::latch       latch{1};
    std::stop_source ss;

    std::unique_ptr<Schedubable, std::function<void(IScheduler*)>> task{new Schedubable, [](auto p) { delete p; }};

    EXPECT_CALL(task->mock, Resume).WillOnce([&] { latch.count_down(); return tinycoro::ETaskResumeState::DONE; });
    EXPECT_CALL(task->mock, SetPauseHandler);

    tinycoro::detail::AtomicQueue<std::unique_ptr<IScheduler, std::function<void(IScheduler*)>>, 128> queue;

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

        std::unique_ptr<Schedubable, std::function<void(IScheduler*)>> task{new Schedubable, [](auto p) { delete p; }};

        EXPECT_CALL(task->mock, Resume)
            .WillOnce([&] { latch.count_down(); return tinycoro::ETaskResumeState::SUSPENDED; })
            .WillOnce([&] { latch.count_down(); return tinycoro::ETaskResumeState::DONE; });
        EXPECT_CALL(task->mock, SetPauseHandler).Times(2);

        tinycoro::detail::AtomicQueue<std::unique_ptr<IScheduler, std::function<void(IScheduler*)>>, 128> queue;

        tinycoro::detail::SchedulerWorker worker{queue, ss.get_token()};

        queue.try_push(std::move(task));

        latch.wait();
        queue.try_push(tinycoro::detail::helper::SCHEDULER_STOP_EVENT);
    }
}