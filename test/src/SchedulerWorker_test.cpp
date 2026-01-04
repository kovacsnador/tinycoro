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

struct SchedulableMock
{
    MOCK_METHOD(tinycoro::ETaskResumeState, Resume, ());

    MOCK_METHOD(std::atomic<tinycoro::EPauseState>&, PauseState, ());

    MOCK_METHOD(void, SetPauseHandler, (tinycoro::ResumeCallback_t));
    MOCK_METHOD(bool, IsObserver, (), (const));
};

struct Schedulable : tinycoro::detail::DoubleLinkable<Schedulable>
{
    tinycoro::ETaskResumeState Resume() { return mock.Resume(); };

    void SetPauseHandler(tinycoro::ResumeCallback_t cb) { mock.SetPauseHandler(cb); };

    auto& PauseState() { return mock.PauseState(); }
    auto  IsObserver() const { return mock.IsObserver(); }

    SchedulableMock mock;

    std::atomic<tinycoro::EPauseState> pauseState{tinycoro::EPauseState::IDLE};
};

TEST(SchedulerWorkerTest, SchedulerWorkerTest_task_execution)
{
    std::latch       latch{1};
    std::stop_source ss;

    std::unique_ptr<Schedulable> task{new Schedulable};

    EXPECT_CALL(task->mock, Resume).WillOnce([&] {
        latch.count_down();
        return tinycoro::ETaskResumeState::DONE;
    });
    EXPECT_CALL(task->mock, SetPauseHandler);

    tinycoro::detail::AtomicQueue<std::unique_ptr<Schedulable>, 128> queue;
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

        std::unique_ptr<Schedulable> task{new Schedulable};

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

        tinycoro::detail::AtomicQueue<std::unique_ptr<Schedulable>, 128> queue;
        tinycoro::detail::Dispatcher                                     dispatcher{queue, ss.get_token()};


        tinycoro::detail::SchedulerWorker worker{dispatcher, ss.get_token()};

        EXPECT_TRUE(dispatcher.try_push(std::move(task)));

        latch.wait();

        ss.request_stop();
        dispatcher.notify_all();
    }
}