#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <coroutine>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/Semaphore.hpp>
#include <tinycoro/Promise.hpp>

template <typename T>
struct SemaphoreMock
{
    MOCK_METHOD(void, Release, ());
    MOCK_METHOD(bool, TryAcquire, (void*, tinycoro::test::CoroutineHandleMock<tinycoro::Promise<T>>));
};

struct SemaphoreAwaiterTest : public testing::Test
{
    using value_type = int32_t;

    SemaphoreAwaiterTest()
    : awaiter{mock}
    {
    }

    void SetUp() override
    {
        hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([]() { /* resumer callback */ });
    }

    SemaphoreMock<value_type>                                          mock;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<value_type>> hdl;

    tinycoro::detail::SemaphoreAwaiter<tinycoro::detail::SemaphoreAwaiterEvent, decltype(mock)> awaiter;
};

TEST_F(SemaphoreAwaiterTest, SemaphoreAwaiterTest_AcquireSucceded)
{
    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(awaiter.next, nullptr);

    EXPECT_CALL(mock, TryAcquire(&awaiter, hdl)).Times(1).WillOnce(testing::Return(true));
    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);

    EXPECT_CALL(mock, Release()).Times(1);
    {
        auto lock = awaiter.await_resume();
    }
}

TEST_F(SemaphoreAwaiterTest, SemaphoreAwaiterTest_AcquireFalied)
{
    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(awaiter.next, nullptr);

    EXPECT_CALL(mock, TryAcquire(&awaiter, hdl)).Times(1).WillOnce(testing::Return(false));
    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());

    EXPECT_CALL(mock, Release()).Times(1);
    {
        auto lock = awaiter.await_resume();
    }
}

template <typename, typename SemaphoreT>
struct AwaiterMock
{
    AwaiterMock(SemaphoreT& s)
    : semaphore{s}
    {
    }

    MOCK_METHOD(void, Notify, (), (const));
    MOCK_METHOD(void, PutOnPause, (tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>>));

    auto TestTryAcquire(auto parentCoro) { return semaphore.TryAcquire(this, parentCoro); }

    auto TestRelease()
    {
        return semaphore.Release();
    }

    AwaiterMock* next;
    SemaphoreT&  semaphore;
};

struct EventMock
{
};

struct SemaphoreTest : public testing::Test
{
    void SetUp() override
    {
        hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([]() { /* resumer callback */ });
    }

    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;
};

TEST_F(SemaphoreTest, SemaphoreTest_counter_1)
{
    tinycoro::detail::SemaphoreType<AwaiterMock, EventMock, tinycoro::detail::LinkedPtrStack> semaphore{1};

    auto mock = semaphore.operator co_await();

    EXPECT_CALL(mock, PutOnPause(hdl)).Times(2);
    EXPECT_TRUE(mock.TestTryAcquire(hdl));
    EXPECT_FALSE(mock.TestTryAcquire(hdl));

    EXPECT_CALL(mock, Notify()).Times(2);
    mock.TestRelease();

    EXPECT_FALSE(mock.TestTryAcquire(hdl));
    mock.TestRelease();
    mock.TestRelease();

    EXPECT_TRUE(mock.TestTryAcquire(hdl));
}