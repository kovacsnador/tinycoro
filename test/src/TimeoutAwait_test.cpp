#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "tinycoro/tinycoro_all.h"

template <typename ReturnT>
struct TestAwaitable
{
    MOCK_METHOD(bool, await_ready, ());
    MOCK_METHOD(ReturnT, await_resume, ());

    // always suspend the coroutine.
    bool await_suspend(auto hdl)
    {
        event.Set(tinycoro::context::PauseTask(hdl));
        return true;
    }

    MOCK_METHOD(void, Notify, ());
    MOCK_METHOD(bool, Cancel, ());

    tinycoro::detail::PauseCallbackEvent event;
};

template <typename T>
struct TimeoutAwaitTest : public testing::Test
{
    using value_type = T;
};

using TimeoutAwaitTestTypes = testing::Types<void, void*, int32_t, std::thread, std::string, std::unique_ptr<int32_t>>;

TYPED_TEST_SUITE(TimeoutAwaitTest, TimeoutAwaitTestTypes);

void TimeoutAwaitTestReturnValue(auto& awaitable)
{
    tinycoro::SoftClock clock;

    auto task = [&]<template <typename> class AwaiterT, typename T>(AwaiterT<T>& awaiter) -> tinycoro::InlineTask<> {
        auto res = co_await tinycoro::TimeoutAwait{clock, std::move(awaiter), 1ms};
        if constexpr (std::same_as<void, T>)
        {
            EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, decltype(res)>));
        }
        else
        {
            EXPECT_TRUE((std::same_as<std::optional<T>, decltype(res)>));
        }
        EXPECT_FALSE(res); // no value
    };

    tinycoro::AllOfInline(task(awaitable));
}

TYPED_TEST(TimeoutAwaitTest, TimeoutAwaitTest_typed)
{
    using T = typename TestFixture::value_type;

    tinycoro::SoftClock clock;
    TestAwaitable<T> awaitable;

    EXPECT_CALL(awaitable, await_ready).Times(1);

    // return that we could cancel the awaitable
    EXPECT_CALL(awaitable, Cancel).WillOnce(testing::Return(true));
    EXPECT_CALL(awaitable, Notify).Times(1).WillOnce([&] { awaitable.event.Notify(); });

    EXPECT_CALL(awaitable, await_resume).Times(0);

    TimeoutAwaitTestReturnValue(awaitable);
}

template <typename AwaitableT, typename TimeT>
void TimeoutAwaitRealTest(AwaitableT&& awaitable, TimeT time)
{
    tinycoro::SoftClock clock;

    auto task = [&]() -> tinycoro::InlineTask<> {
        [[maybe_unused]] auto start = clock.Now();

        // This is now cancellable with a timeout
        auto res = co_await tinycoro::TimeoutAwait{clock, std::move(awaitable), time};

        EXPECT_FALSE(res.has_value());

        if constexpr (tinycoro::concepts::IsDuration<TimeT>)
        {
            // at least x time is elapsed
            EXPECT_TRUE(start + 100ms <= clock.Now());
        }
        else
        {
            EXPECT_GE(clock.Now(), time);
        }
    };

    tinycoro::AllOfInline(task());
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_ManualEvent)
{
    tinycoro::ManualEvent event{};

    TimeoutAwaitRealTest(event.Wait(), 100ms);
    TimeoutAwaitRealTest(event.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_AutoEvent)
{
    TimeoutAwaitRealTest(tinycoro::AutoEvent{}.Wait(), 100ms);
    TimeoutAwaitRealTest(tinycoro::AutoEvent{}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_SingleEvent)
{
    TimeoutAwaitRealTest(tinycoro::SingleEvent<int32_t>{}.Wait(), 100ms);
    TimeoutAwaitRealTest(tinycoro::SingleEvent<int32_t>{}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_Barrier)
{
    TimeoutAwaitRealTest(tinycoro::Barrier{3}.Wait(), 100ms);
    TimeoutAwaitRealTest(tinycoro::Barrier{3}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_Latch)
{
    TimeoutAwaitRealTest(tinycoro::Latch{3}.Wait(), 100ms);
    TimeoutAwaitRealTest(tinycoro::Latch{3}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_pop_wait)
{
    tinycoro::BufferedChannel<int32_t> channel;
    int32_t                            val{};

    TimeoutAwaitRealTest(channel.PopWait(val), 100ms);
    TimeoutAwaitRealTest(channel.PopWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_push_wait)
{
    tinycoro::BufferedChannel<int32_t> channel{2};
    int32_t                            val{};

    channel.Push(41);
    channel.Push(42);

    EXPECT_FALSE(channel.Empty());

    TimeoutAwaitRealTest(channel.PushWait(val), 100ms);
    TimeoutAwaitRealTest(channel.PushWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_push_and_close_wait)
{
    tinycoro::BufferedChannel<int32_t> channel{2};
    int32_t                            val{};

    channel.Push(41);
    channel.Push(42);

    EXPECT_FALSE(channel.Empty());

    TimeoutAwaitRealTest(channel.PushAndCloseWait(val), 100ms);
    TimeoutAwaitRealTest(channel.PushAndCloseWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_wait_for_listeners)
{
    tinycoro::BufferedChannel<int32_t> channel{2};

    TimeoutAwaitRealTest(channel.WaitForListeners(1), 100ms);
    TimeoutAwaitRealTest(channel.WaitForListeners(1), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_UnbufferedChannel_pop_wait)
{
    tinycoro::UnbufferedChannel<int32_t> channel;
    int32_t                              val{};

    TimeoutAwaitRealTest(channel.PopWait(val), 100ms);
    TimeoutAwaitRealTest(channel.PopWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_UnbufferedChannel_push_wait)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    TimeoutAwaitRealTest(channel.PushWait(42), 100ms);
    TimeoutAwaitRealTest(channel.PushWait(43), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_UnbufferedChannel_push_and_close_wait)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    TimeoutAwaitRealTest(channel.PushAndCloseWait(42), 100ms);
    TimeoutAwaitRealTest(channel.PushAndCloseWait(43), tinycoro::SoftClock::Now() + 100ms);
}