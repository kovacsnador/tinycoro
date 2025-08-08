#include <gtest/gtest.h>

#include "tinycoro/tinycoro_all.h"

struct TestAwaitable
{
    
};


template<typename AwaitableT, typename TimeT>
void TimeoutAwaitTest(AwaitableT&& awaitable, TimeT time)
{
    tinycoro::SoftClock clock; 

    auto task = [&]()->tinycoro::InlineTask<> {

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

    TimeoutAwaitTest(event.Wait(), 100ms);
    TimeoutAwaitTest(event.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_AutoEvent)
{
    TimeoutAwaitTest(tinycoro::AutoEvent{}.Wait(), 100ms);
    TimeoutAwaitTest(tinycoro::AutoEvent{}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_SingleEvent)
{
    TimeoutAwaitTest(tinycoro::SingleEvent<int32_t>{}.Wait(), 100ms);
    TimeoutAwaitTest(tinycoro::SingleEvent<int32_t>{}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_Barrier)
{
    TimeoutAwaitTest(tinycoro::Barrier{3}.Wait(), 100ms);
    TimeoutAwaitTest(tinycoro::Barrier{3}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_Latch)
{
    TimeoutAwaitTest(tinycoro::Latch{3}.Wait(), 100ms);
    TimeoutAwaitTest(tinycoro::Latch{3}.Wait(), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_pop_wait)
{
    tinycoro::BufferedChannel<int32_t> channel;
    int32_t val{};

    TimeoutAwaitTest(channel.PopWait(val), 100ms);
    TimeoutAwaitTest(channel.PopWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_push_wait)
{
    tinycoro::BufferedChannel<int32_t> channel{2};
    int32_t val{};

    channel.Push(41);
    channel.Push(42);

    EXPECT_FALSE(channel.Empty());

    TimeoutAwaitTest(channel.PushWait(val), 100ms);
    TimeoutAwaitTest(channel.PushWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_push_and_close_wait)
{
    tinycoro::BufferedChannel<int32_t> channel{2};
    int32_t val{};

    channel.Push(41);
    channel.Push(42);

    EXPECT_FALSE(channel.Empty());

    TimeoutAwaitTest(channel.PushAndCloseWait(val), 100ms);
    TimeoutAwaitTest(channel.PushAndCloseWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_BufferedChannel_wait_for_listeners)
{
    tinycoro::BufferedChannel<int32_t> channel{2};

    TimeoutAwaitTest(channel.WaitForListeners(1), 100ms);
    TimeoutAwaitTest(channel.WaitForListeners(1), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_UnbufferedChannel_pop_wait)
{
    tinycoro::UnbufferedChannel<int32_t> channel;
    int32_t val{};

    TimeoutAwaitTest(channel.PopWait(val), 100ms);
    TimeoutAwaitTest(channel.PopWait(val), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_UnbufferedChannel_push_wait)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    TimeoutAwaitTest(channel.PushWait(42), 100ms);
    TimeoutAwaitTest(channel.PushWait(43), tinycoro::SoftClock::Now() + 100ms);
}

TEST(TimeoutAwaitTest, TimeoutAwaitTest_UnbufferedChannel_push_and_close_wait)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    TimeoutAwaitTest(channel.PushAndCloseWait(42), 100ms);
    TimeoutAwaitTest(channel.PushAndCloseWait(43), tinycoro::SoftClock::Now() + 100ms);
}