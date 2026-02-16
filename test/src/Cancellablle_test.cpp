#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

struct CancellableAwaiter_Mock
{
    MOCK_METHOD(bool, await_ready, ());
    MOCK_METHOD(bool, await_suspend, (std::coroutine_handle<>));
    MOCK_METHOD(void, await_resume, ());

    MOCK_METHOD(bool, Cancel, ());
    MOCK_METHOD(void, Notify, ());
    MOCK_METHOD(void, NotifyToDestroy, ());
};

TEST(CancellableTest, CancellableTest_mock_await_ready)
{
    CancellableAwaiter_Mock mock;

    EXPECT_CALL(mock, await_ready()).Times(1).WillOnce(testing::Return(true));

    tinycoro::Cancellable cancellable{std::move(mock)};
    EXPECT_TRUE(cancellable.await_ready());
}

TEST(CancellableTest, CancellableTest_mock_await_suspend)
{
    CancellableAwaiter_Mock mock;
    auto                    hdl = tinycoro::test::MakeCoroutineHdl();

    auto stopSource = hdl.promise().StopSource();

    EXPECT_CALL(mock, await_suspend).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock, Cancel).WillOnce(testing::Return(true));
    EXPECT_CALL(mock, NotifyToDestroy).Times(1);

    tinycoro::Cancellable cancellable{std::move(mock)};
    EXPECT_TRUE(cancellable.await_suspend(hdl));

    stopSource.request_stop();
}

TEST(CancellableTest, CancellableTest_mock_await_suspend_custom_token)
{
    CancellableAwaiter_Mock mock;
    auto                    hdl = tinycoro::test::MakeCoroutineHdl();

    auto stopSource = hdl.promise().StopSource();

    std::stop_source ss;

    EXPECT_CALL(mock, await_suspend).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock, Cancel).WillOnce(testing::Return(true));
    EXPECT_CALL(mock, NotifyToDestroy).Times(1);

    tinycoro::Cancellable cancellable{ss.get_token(), std::move(mock)};
    EXPECT_TRUE(cancellable.await_suspend(hdl));

    ss.request_stop();
    EXPECT_TRUE(stopSource.stop_requested());
}

TEST(CancellableTest, CancellableTest_mock_await_suspend_false)
{
    CancellableAwaiter_Mock mock;
    auto                    hdl = tinycoro::test::MakeCoroutineHdl();

    auto stopSource = hdl.promise().StopSource();

    EXPECT_CALL(mock, await_suspend).Times(1).WillOnce(testing::Return(false));
    EXPECT_CALL(mock, Cancel).Times(0);
    EXPECT_CALL(mock, Notify).Times(0);

    tinycoro::Cancellable cancellable{std::move(mock)};
    EXPECT_FALSE(cancellable.await_suspend(hdl));

    stopSource.request_stop();
}

TEST(CancellableTest, CancellableTest_mock_await_suspend_false_custom_token)
{
    CancellableAwaiter_Mock mock;
    auto                    hdl = tinycoro::test::MakeCoroutineHdl();

    auto stopSource = hdl.promise().StopSource();

    std::stop_source ss;

    EXPECT_CALL(mock, await_suspend).Times(1).WillOnce(testing::Return(false));
    EXPECT_CALL(mock, Cancel).Times(0);
    EXPECT_CALL(mock, Notify).Times(0);

    tinycoro::Cancellable cancellable{ss.get_token(), std::move(mock)};
    EXPECT_FALSE(cancellable.await_suspend(hdl));

    ss.request_stop();
    EXPECT_FALSE(stopSource.stop_requested());
}

TEST(CancellableTest, CancellableTest_mock_await_resume)
{
    CancellableAwaiter_Mock mock;

    EXPECT_CALL(mock, await_resume()).Times(1);

    tinycoro::Cancellable cancellable{std::move(mock)};
    cancellable.await_resume();
}

TEST(CancellableTest, CancellableTest_with_timeout)
{
    tinycoro::SoftClock clock;
    tinycoro::AutoEvent event;

    auto task = [&]() -> tinycoro::Task<> { co_await tinycoro::TimeoutAwait{clock, tinycoro::Cancellable{event.Wait()}, 100ms}; };

    tinycoro::AllOf(task());

    EXPECT_FALSE(event.IsSet());
}

TEST(CancellableTest, CancellableTest_with_custom_token)
{
    tinycoro::AutoEvent event;
    std::stop_source    ss;

    auto task = [&]() -> tinycoro::Task<> { co_await tinycoro::Cancellable{ss.get_token(), event.Wait()}; };

    auto task2 = [&]() -> tinycoro::Task<> {
        ss.request_stop();
        co_return;
    };

    tinycoro::AllOf(task(), task2());

    EXPECT_FALSE(event.IsSet());
    EXPECT_TRUE(ss.stop_requested());
}

struct CancellableStressTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(CancellableStressTest, CancellableStressTest, testing::Values(1, 10, 100, 1000, 10000, 20000));

TEST_P(CancellableStressTest, CancellableTest_with_timeout)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;
    tinycoro::AutoEvent event;

    auto task = [&]() -> tinycoro::TaskNIC<> { co_await tinycoro::TimeoutAwait{clock, tinycoro::Cancellable{event.Wait()}, 20ms}; };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count);
    for (size_t i=0; i < count; ++i)
        tasks.push_back(task());

    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_FALSE(event.IsSet());
}