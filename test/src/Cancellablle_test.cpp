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
    auto hdl = tinycoro::test::MakeCoroutineHdl([]{});

    auto stopSource = hdl.promise().StopSource();

    EXPECT_CALL(mock, await_suspend).Times(1).WillOnce(testing::Return(true));
    EXPECT_CALL(mock, Cancel).WillOnce(testing::Return(true));
    EXPECT_CALL(mock, Notify).Times(1);

    tinycoro::Cancellable cancellable{std::move(mock)};
    EXPECT_TRUE(cancellable.await_suspend(hdl));

    stopSource.request_stop();
}

TEST(CancellableTest, CancellableTest_mock_await_suspend_false)
{
    CancellableAwaiter_Mock mock;
    auto hdl = tinycoro::test::MakeCoroutineHdl([]{});

    auto stopSource = hdl.promise().StopSource();

    EXPECT_CALL(mock, await_suspend).Times(1).WillOnce(testing::Return(false));
    EXPECT_CALL(mock, Cancel).Times(0);
    EXPECT_CALL(mock, Notify).Times(0);

    tinycoro::Cancellable cancellable{std::move(mock)};
    EXPECT_FALSE(cancellable.await_suspend(hdl));

    stopSource.request_stop();
}

TEST(CancellableTest, CancellableTest_mock_await_resume)
{
    CancellableAwaiter_Mock mock;

    EXPECT_CALL(mock, await_resume()).Times(1);

    tinycoro::Cancellable cancellable{std::move(mock)};
    cancellable.await_resume();
}