#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <latch>

#include <tinycoro/tinycoro_all.h>

TEST(RequestStopForQueueTest, RequestStopForQueue_fullQueue)
{
    int32_t val{42};

    tinycoro::detail::AtomicQueue<int32_t*, 2> queue;

    tinycoro::detail::Dispatcher dispatcher{queue, {}};

    EXPECT_TRUE(dispatcher.try_push(&val));
    EXPECT_TRUE(dispatcher.try_push(&val));

    EXPECT_TRUE(dispatcher.full());

    dispatcher.notify_all();

    EXPECT_TRUE(dispatcher.full());
}

struct AtomicQueueMock
{
    using value_type = int32_t*;

    MOCK_METHOD(bool, try_pop, (int32_t*));
    MOCK_METHOD(bool, try_push, (int32_t*));
    MOCK_METHOD(bool, full, ());
    MOCK_METHOD(void, notify_push_waiters, ());
    MOCK_METHOD(void, notify_pop_waiters, ());
};

struct SchedubableMock
{
    MOCK_METHOD(tinycoro::detail::ETaskResumeState, Resume, ());

    MOCK_METHOD(std::atomic<tinycoro::detail::EPauseState>&, PauseState, ());

    MOCK_METHOD(void, SetResumeCallback, (tinycoro::ResumeCallback_t));

    MOCK_METHOD(tinycoro::detail::SharedState*, SharedState, ());
};

struct Schedubable : tinycoro::detail::DoubleLinkable<Schedubable>
{
    auto Resume() { return mock.Resume(); };

    void SetResumeCallback(tinycoro::ResumeCallback_t cb) { mock.SetResumeCallback(cb); };

    auto SharedState() { return mock.SharedState(); }

    SchedubableMock mock;

    tinycoro::detail::SharedState sharedState{false};
};

TEST(SchedulerWorkerTest, SchedulerWorkerTest_task_execution)
{
    std::latch       latch{1};
    std::stop_source ss;

    std::unique_ptr<Schedubable> task{new Schedubable};

    EXPECT_CALL(task->mock, Resume).WillOnce([&] {
        latch.count_down();
        return tinycoro::detail::ETaskResumeState::DONE;
    });
    EXPECT_CALL(task->mock, SetResumeCallback);

    tinycoro::detail::AtomicQueue<std::unique_ptr<Schedubable>, 128> queue;
    tinycoro::detail::Dispatcher                                     dispatcher{queue, ss.get_token()};

    tinycoro::detail::SchedulerWorker worker{dispatcher, ss.get_token()};

    EXPECT_TRUE(dispatcher.try_push(std::move(task)));

    latch.wait();

    ss.request_stop();
    dispatcher.notify_all();
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
                return tinycoro::detail::ETaskResumeState::SUSPENDED;
            })
            .WillOnce([&] {
                latch.count_down();
                return tinycoro::detail::ETaskResumeState::DONE;
            });
        EXPECT_CALL(task->mock, SetResumeCallback).Times(2);

        tinycoro::detail::AtomicQueue<std::unique_ptr<Schedubable>, 128> queue;
        tinycoro::detail::Dispatcher                                     dispatcher{queue, ss.get_token()};


        tinycoro::detail::SchedulerWorker worker{dispatcher, ss.get_token()};

        EXPECT_TRUE(dispatcher.try_push(std::move(task)));

        latch.wait();

        ss.request_stop();
        dispatcher.notify_all();
    }
}