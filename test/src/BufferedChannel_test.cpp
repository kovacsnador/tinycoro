#include <gtest/gtest.h>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(BufferedChannelTest, BufferedChannelTest_empty)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.Empty());
    channel.Push(42);
    EXPECT_FALSE(channel.Empty());

    int32_t val;
    auto    awaiter = channel.PopWait(val);
    EXPECT_TRUE(awaiter.await_ready());

    EXPECT_TRUE(channel.Empty());
}

TEST(BufferedChannelTest, BufferedChannelTest_defaultConstructor)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_EQ(channel.MaxSize(), std::numeric_limits<size_t>::max());
}

TEST(BufferedChannelTest, BufferedChannelTest_constructorException)
{
    EXPECT_THROW(tinycoro::BufferedChannel<int32_t> channel{0}, tinycoro::BufferedChannelException);
}

TEST(BufferedChannelTest, BufferedChannelTest_open_push)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Push(42);
    EXPECT_TRUE(channel.IsOpen());

    channel.PushAndClose(44);
    EXPECT_TRUE(channel.IsOpen());

    int32_t val;
    auto    awaiter = channel.PopWait(val);

    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_TRUE(channel.IsOpen());
    EXPECT_EQ(val, 42);

    auto awaiter2 = channel.PopWait(val);

    EXPECT_TRUE(awaiter2.await_ready());
    EXPECT_FALSE(channel.IsOpen()); // channel need to be closed
    EXPECT_EQ(val, 44);
}

TEST(BufferedChannelTest, BufferedChannelTest_open_push_await_suspend)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Push(42);
    EXPECT_TRUE(channel.IsOpen());

    channel.PushAndClose(44);
    EXPECT_TRUE(channel.IsOpen());

    int32_t val;
    auto    awaiter1 = channel.PopWait(val);

    auto hdl1 = tinycoro::test::MakeCoroutineHdl();

    EXPECT_FALSE(awaiter1.await_suspend(hdl1));
    EXPECT_TRUE(channel.IsOpen());
    EXPECT_EQ(val, 42);

    auto awaiter2 = channel.PopWait(val);

    auto hdl2 = tinycoro::test::MakeCoroutineHdl();

    EXPECT_FALSE(awaiter2.await_suspend(hdl2));
    EXPECT_FALSE(channel.IsOpen()); // channel need to be closed

    EXPECT_THROW(channel.Push(33), tinycoro::BufferedChannelException);

    EXPECT_EQ(val, 44);
}

TEST(BufferedChannelTest, BufferedChannelTest_open_emplace_await_suspend)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Push(42);
    EXPECT_TRUE(channel.IsOpen());

    channel.PushAndClose(44);
    EXPECT_TRUE(channel.IsOpen());

    int32_t val;
    auto    awaiter1 = channel.PopWait(val);

    auto hdl = tinycoro::test::MakeCoroutineHdl();

    EXPECT_FALSE(awaiter1.await_suspend(hdl));
    EXPECT_TRUE(channel.IsOpen());
    EXPECT_EQ(val, 42);

    auto awaiter2 = channel.PopWait(val);

    auto hdl2 = tinycoro::test::MakeCoroutineHdl();

    EXPECT_FALSE(awaiter2.await_suspend(hdl2));
    EXPECT_FALSE(channel.IsOpen()); // channel need to be closed

    EXPECT_THROW(channel.PushAndClose(33), tinycoro::BufferedChannelException);

    EXPECT_EQ(val, 44);
}

TEST(BufferedChannelTest, BufferedChannelTest_moveOnlyValue)
{
    struct MoveOnly
    {
        MoveOnly() = default;

        MoveOnly(int32_t v)
        : value{v}
        {
        }

        MoveOnly(MoveOnly&& other) noexcept
        : value{std::exchange(other.value, 0)}
        {
        }

        MoveOnly& operator=(MoveOnly&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                value = std::exchange(other.value, 0);
            }
            return *this;
        }

        int32_t value{};
    };

    tinycoro::BufferedChannel<MoveOnly> channel;
    EXPECT_TRUE(channel.Empty());
    channel.Push(42);
    EXPECT_FALSE(channel.Empty());

    MoveOnly val;
    auto     awaiter = channel.PopWait(val);
    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_EQ(val.value, 42);

    EXPECT_TRUE(channel.Empty());

    // pushWait to use push awaiter
    auto pushAwaiter = channel.PushWait(44);
    EXPECT_TRUE(pushAwaiter.await_ready());

    EXPECT_FALSE(channel.Empty());

    auto awaiter2 = channel.PopWait(val);
    EXPECT_TRUE(awaiter2.await_ready());

    EXPECT_TRUE(channel.Empty());

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
    EXPECT_EQ(44, val.value);
}

template <typename T, typename U, typename V>
class PopAwaiterMock : public tinycoro::detail::SingleLinkable<PopAwaiterMock<T, U, V>>
{
public:
    PopAwaiterMock(auto&, auto, auto) { }

    bool Notify() const noexcept { return true; };
};

template <typename C, typename E>
class ListenerAwaiterMock : public tinycoro::detail::SingleLinkable<ListenerAwaiterMock<C, E>>
{
public:
    ListenerAwaiterMock(C&, E, size_t v)
    : val{v}
    {
    }

    bool Notify() const noexcept { return true; };

    auto value() { return val; }

    size_t val;
};

template <typename T, typename U, typename V>
class PushAwaiterMock : public tinycoro::detail::SingleLinkable<PushAwaiterMock<T, U, V>>
{
public:
    PushAwaiterMock(auto&, auto...) { }

    bool Notify() const noexcept { return true; }
};

TEST(BufferedChannelTest, BufferedChannelTest_coawaitReturn)
{
    tinycoro::detail::BufferedChannel<int32_t, PopAwaiterMock, ListenerAwaiterMock, PushAwaiterMock, tinycoro::detail::Queue> channel;

    int32_t val;
    auto    awaiter = channel.PopWait(val);

    using expectedAwaiterType = PopAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BufferedChannelTest, BufferedChannelTest_coawait_listenerWaiter)
{
    tinycoro::detail::BufferedChannel<int32_t, PopAwaiterMock, ListenerAwaiterMock, PushAwaiterMock, tinycoro::detail::Queue> channel;

    auto awaiter = channel.WaitForListeners(1);

    using expectedAwaiterType = ListenerAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BufferedChannelTest, BufferedChannelTest_await_ready)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_TRUE(channel.IsOpen());
    EXPECT_EQ(val, 0);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_ready_listener)
{
    tinycoro::BufferedChannel<int32_t> channel;

    auto listenerAwaiter = channel.WaitForListeners(1);

    EXPECT_FALSE(listenerAwaiter.await_ready());
    EXPECT_TRUE(channel.IsOpen());
}

TEST(BufferedChannelTest, BufferedChannelTest_await_ready_listener_closed)
{
    tinycoro::BufferedChannel<int32_t> channel;

    channel.Close();

    auto listenerAwaiter = channel.WaitForListeners(1);
    EXPECT_TRUE(listenerAwaiter.await_ready());
}

TEST(BufferedChannelTest, BufferedChannelTest_await_ready_listener_closed_after)
{
    tinycoro::BufferedChannel<int32_t> channel;

    auto listenerAwaiter = channel.WaitForListeners(1);
    EXPECT_FALSE(listenerAwaiter.await_ready());

    channel.Close();

    auto hdl = tinycoro::test::MakeCoroutineHdl();
    EXPECT_FALSE(listenerAwaiter.await_suspend(hdl));
}

TEST(BufferedChannelTest, BufferedChannelTest_cancel)
{
    tinycoro::Scheduler                scheduler;
    tinycoro::SoftClock                clock;
    tinycoro::BufferedChannel<int32_t> channel;

    auto listener = [&]() -> tinycoro::TaskNIC<int32_t> {
        int32_t val;
        co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return 42;
    };

    auto listenerWaiter = [&]() -> tinycoro::TaskNIC<void> { co_await tinycoro::Cancellable(channel.WaitForListeners(10)); };

    auto [r1, r2, r3, r4, r5, r6, r7, r8, r9] = tinycoro::AnyOf(scheduler,
                                                                listener(),
                                                                listener(),
                                                                listener(),
                                                                listener(),
                                                                listenerWaiter(),
                                                                listenerWaiter(),
                                                                listenerWaiter(),
                                                                listenerWaiter(),
                                                                tinycoro::SleepFor(clock, 100ms));

    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());
    EXPECT_FALSE(r6.has_value());
    EXPECT_FALSE(r7.has_value());
    EXPECT_FALSE(r8.has_value());
    EXPECT_TRUE(r9.has_value());
}

TEST(BufferedChannelTest, BufferedChannelTest_cancel_inline)
{
    tinycoro::SoftClock                clock;
    tinycoro::BufferedChannel<int32_t> channel;

    auto listener = [&]() -> tinycoro::TaskNIC<int32_t> {
        int32_t val;
        co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return 42;
    };

    auto listenerWaiter = [&]() -> tinycoro::TaskNIC<void> { co_await tinycoro::Cancellable(channel.WaitForListeners(10)); };

    auto [r1, r2, r3, r4, r5, r6, r7, r8, r9] = tinycoro::AnyOf(listener(),
                                                                      listener(),
                                                                      listener(),
                                                                      listener(),
                                                                      listenerWaiter(),
                                                                      listenerWaiter(),
                                                                      listenerWaiter(),
                                                                      listenerWaiter(),
                                                                      tinycoro::SleepFor(clock, 100ms));

    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());
    EXPECT_FALSE(r6.has_value());
    EXPECT_FALSE(r7.has_value());
    EXPECT_FALSE(r8.has_value());
    EXPECT_TRUE(r9.has_value());
}

using ListenerTestData = std::tuple<size_t, bool>;

struct BufferedChannelListenerTest : testing::TestWithParam<ListenerTestData>
{
};

INSTANTIATE_TEST_SUITE_P(BufferedChannelListenerTest,
                         BufferedChannelListenerTest,
                         testing::Values(ListenerTestData{1, true},
                                         ListenerTestData{2, true},
                                         ListenerTestData{3, true},
                                         ListenerTestData{4, false},
                                         ListenerTestData{10, false}));

TEST_P(BufferedChannelListenerTest, BufferedChannelTest_await_ready_with_listener)
{
    auto [count, ready] = GetParam();

    tinycoro::BufferedChannel<int32_t> channel;

    auto    hdl1 = tinycoro::test::MakeCoroutineHdl();
    int32_t val1{};
    auto    awaiter1 = channel.PopWait(val1);
    EXPECT_TRUE(awaiter1.await_suspend(hdl1));

    auto    hdl2 = tinycoro::test::MakeCoroutineHdl();
    int32_t val2{};
    auto    awaiter2 = channel.PopWait(val2);
    EXPECT_TRUE(awaiter2.await_suspend(hdl2));

    auto    hdl3 = tinycoro::test::MakeCoroutineHdl();
    int32_t val3{};
    auto    awaiter3 = channel.PopWait(val3);
    EXPECT_TRUE(awaiter3.await_suspend(hdl3));

    auto listenerAwaiter = channel.WaitForListeners(count);

    // we have already 3 awaiter so listeners are listening
    EXPECT_EQ(listenerAwaiter.await_ready(), ready);
    EXPECT_TRUE(channel.IsOpen());

    // because of the awaiters registration close is necessary here.
    channel.Close();
}

TEST_P(BufferedChannelListenerTest, BufferedChannelTest_await_suspend_with_listener)
{
    auto [count, ready] = GetParam();

    tinycoro::BufferedChannel<int32_t> channel;

    auto    hdl1 = tinycoro::test::MakeCoroutineHdl();
    int32_t val1{};
    auto    awaiter1 = channel.PopWait(val1);
    EXPECT_TRUE(awaiter1.await_suspend(hdl1));

    auto    hdl2 = tinycoro::test::MakeCoroutineHdl();
    int32_t val2{};
    auto    awaiter2 = channel.PopWait(val2);
    EXPECT_TRUE(awaiter2.await_suspend(hdl2));

    auto    hdl3 = tinycoro::test::MakeCoroutineHdl();
    int32_t val3{};
    auto    awaiter3 = channel.PopWait(val3);
    EXPECT_TRUE(awaiter3.await_suspend(hdl3));

    auto listenerAwaiter = channel.WaitForListeners(count);

    auto listenerHdl = tinycoro::test::MakeCoroutineHdl();

    // we have already 3 awaiter so listeners are listening
    EXPECT_NE(listenerAwaiter.await_suspend(listenerHdl), ready);
    EXPECT_TRUE(channel.IsOpen());

    // because of the awaiters registration close is necessary here.
    channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelTest_await_ready_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    channel.PushAndClose(42);

    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_FALSE(channel.IsOpen());
    EXPECT_EQ(val, 42);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_suspend)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    bool pauseResumeCalled{false};
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled](auto) { pauseResumeCalled = true; });

    EXPECT_TRUE(awaiter.await_suspend(hdl));
    EXPECT_EQ(val, 0);

    channel.Push(42);
    EXPECT_TRUE(pauseResumeCalled);
    EXPECT_EQ(val, 42);

    // because of the awaiters registration close is necessary here.
    channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    bool pauseResumeCalled{false};
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled](auto) { pauseResumeCalled = true; });

    EXPECT_TRUE(awaiter.await_suspend(hdl));
    EXPECT_EQ(val, 0);

    channel.Push(42);

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
    EXPECT_EQ(val, 42);

    // because of the awaiters registration close is necessary here.
    channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelTest_tryPush)
{
    tinycoro::BufferedChannel<int32_t> channel{2};

    EXPECT_TRUE(channel.TryPush(40));
    EXPECT_TRUE(channel.TryPush(41));

    EXPECT_FALSE(channel.TryPush(42));
    EXPECT_FALSE(channel.TryPush(43));

    EXPECT_TRUE(channel.IsOpen());
}

TEST(BufferedChannelTest, BufferedChannelTest_tryPushAndClose)
{
    tinycoro::BufferedChannel<int32_t> channel{2};

    EXPECT_TRUE(channel.IsOpen());

    EXPECT_TRUE(channel.TryPushAndClose(41));

    // still need to be open, we still waiting for pop awaiters to get the value.
    EXPECT_TRUE(channel.IsOpen());

    EXPECT_TRUE(channel.TryPush(42));

    int32_t val;
    auto    awaiter = channel.PopWait(val);
    EXPECT_TRUE(awaiter.await_ready());

    // get the last value
    EXPECT_EQ(val, 41);

    // after getting the last value, the channel is in closed state.
    EXPECT_FALSE(channel.IsOpen());
    EXPECT_THROW(channel.Push(42), tinycoro::BufferedChannelException);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume_push_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    auto hdl = tinycoro::test::MakeCoroutineHdl();

    channel.Push(42);

    EXPECT_FALSE(awaiter.await_suspend(hdl));
    EXPECT_EQ(val, 42);

    channel.Close();

    auto result = awaiter.await_resume();

    // the value was already set
    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
    EXPECT_EQ(val, 42);

    // for the next is already closed
    auto awaiter2 = channel.PopWait(val);
    EXPECT_EQ(tinycoro::EChannelOpStatus::CLOSED, awaiter2.await_resume());
}

TEST(BufferedChannelTest, BufferedChannelFunctionalTest_cleanup_callback)
{
    std::vector<size_t> coll;
    auto                cleanup = [&coll](auto& val) { coll.push_back(val); };

    tinycoro::BufferedChannel<size_t> channel{cleanup};

    channel.Push(40u);
    channel.Push(41u);
    channel.Push(42u);
    channel.Push(43u);
    channel.Push(44u);

    channel.Close();

    EXPECT_EQ(coll.size(), 5);

    size_t expected = 44;
    for (const auto& it : coll)
    {
        EXPECT_EQ(expected--, it);
    }
}

TEST(BufferedChannelTest, BufferedChannelFunctionalTest_cleanup_callback_pushWait)
{
    std::vector<size_t> coll;
    auto                cleanup = [&coll](auto& val) { coll.push_back(val); };

    tinycoro::BufferedChannel<size_t> channel{cleanup};

    auto producer = [&](size_t val) -> tinycoro::Task<> { co_await channel.PushWait(val); };

    auto consumer = [&](size_t expected) -> tinycoro::Task<> {
        size_t val;
        std::ignore = co_await channel.PopWait(val);
        EXPECT_EQ(val, expected);
    };

    tinycoro::AllOf(producer(40), producer(41), producer(42), producer(43), producer(44), consumer(40), consumer(41));

    channel.Close();

    EXPECT_EQ(coll.size(), 3);

    size_t expected = 44;
    for (const auto& it : coll)
    {
        EXPECT_EQ(expected--, it);
    }
}

TEST(BufferedChannelTest, BufferedChannelFunctionalTest_cleanup_callback_stuck_pushawaiter)
{
    std::vector<size_t> coll;
    auto                cleanup = [&coll](auto& val) { coll.push_back(val); };

    tinycoro::BufferedChannel<size_t> channel{1, cleanup};

    auto producer = [&](size_t val) -> tinycoro::Task<> { co_await channel.PushWait(val); };

    auto consumer = [&](size_t expected) -> tinycoro::Task<> {
        size_t val;
        std::ignore = co_await channel.PopWait(val);
        EXPECT_EQ(val, expected);
    };

    auto closer = [&]() -> tinycoro::Task<> {
        channel.Close();
        co_return;
    };

    tinycoro::AllOf(producer(40), producer(41), producer(42), producer(43), producer(44), consumer(40), consumer(41), closer());

    EXPECT_EQ(coll.size(), 3);

    size_t expected = 42;
    for (const auto& it : coll)
    {
        EXPECT_EQ(expected++, it);
    }
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    auto hdl = tinycoro::test::MakeCoroutineHdl();

    EXPECT_TRUE(awaiter.await_suspend(hdl));

    channel.Close();

    auto result = awaiter.await_resume();
    // the value was not set
    EXPECT_EQ(tinycoro::EChannelOpStatus::CLOSED, result);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume_multi)
{
    tinycoro::BufferedChannel<int32_t> channel;

    channel.Push(41);
    channel.Push(42);
    channel.Push(43);
    channel.Push(44);

    for (size_t i = 41; i < 45; ++i)
    {
        int32_t val{};
        auto    awaiter = channel.PopWait(val);

        bool pauseResumeCalled{false};
        auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled](auto) { pauseResumeCalled = true; });

        EXPECT_FALSE(awaiter.await_suspend(hdl));

        auto result = awaiter.await_resume();
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
        EXPECT_EQ(val, i);
    }
}

TEST(BufferedChannelTest, BufferedChannelTest_push_await)
{
    tinycoro::BufferedChannel<int32_t> channel;

    auto pushAwaiter = channel.PushWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_TRUE(popAwaiter.await_ready());

    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, popAwaiter.await_resume());
    EXPECT_EQ(val, 42);
}

TEST(BufferedChannelTest, BufferedChannelTest_push_await_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    channel.Close();

    auto pushAwaiter = channel.PushWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());
    EXPECT_EQ(tinycoro::EChannelOpStatus::CLOSED, pushAwaiter.await_resume());
}

TEST(BufferedChannelTest, BufferedChannelTest_push_await_close_after)
{
    tinycoro::BufferedChannel<int32_t> channel{1};

    // the channel is full
    channel.Push(41);

    auto pushAwaiter = channel.PushWait(42);
    EXPECT_FALSE(pushAwaiter.await_ready());

    channel.Close();

    auto hdl = tinycoro::test::MakeCoroutineHdl();
    EXPECT_FALSE(pushAwaiter.await_suspend(hdl));

    EXPECT_EQ(tinycoro::EChannelOpStatus::CLOSED, pushAwaiter.await_resume());
}

TEST(BufferedChannelTest, BufferedChannelTest_waitForce)
{
    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler;

    tinycoro::BufferedChannel<int32_t> channel{2};

    channel.Push(1);
    channel.Push(2);

    tinycoro::SoftClock::timepoint_t start;

    auto consumer = [&](auto sleepDuration) -> tinycoro::Task<void> {
        start = clock.Now();
        co_await tinycoro::SleepFor(clock, sleepDuration);

        int32_t val;
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await channel.PopWait(val));

        EXPECT_EQ(val, 1);
    };

    auto producer = [&](auto sleepDuration) -> tinycoro::Task<void> {
        // this is a blocker push
        channel.Push(3);

        EXPECT_TRUE(clock.Now() - start >= sleepDuration);

        co_return;
    };

    auto duration = 200ms;

    tinycoro::AllOf(scheduler, producer(duration), consumer(duration));
}

TEST(BufferedChannelTest, BufferedChannelTest_push_await_order)
{
    tinycoro::BufferedChannel<int32_t> channel{3};

    channel.Push(1);
    channel.Push(2);
    channel.Push(3);

    auto pushAwaiter_4 = channel.PushWait(4);
    EXPECT_FALSE(pushAwaiter_4.await_ready());

    auto hdl_4 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(pushAwaiter_4.await_suspend(hdl_4));

    auto pushAwaiter_5 = channel.PushWait(5);
    EXPECT_FALSE(pushAwaiter_5.await_ready());

    auto hdl_5 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(pushAwaiter_5.await_suspend(hdl_5));

    auto popValue = [&](int32_t expected) {
        int32_t val;
        auto    popAwaiter = channel.PopWait(val);
        EXPECT_TRUE(popAwaiter.await_ready());
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, popAwaiter.await_resume());
        EXPECT_EQ(val, expected);
    };

    popValue(1);
    popValue(2);
    popValue(3);
    popValue(4);
    popValue(5);

    EXPECT_TRUE(channel.Size() == 0);

    auto pushWait = [&](int32_t expected) {
        auto pushAwaiter = channel.PushWait(expected);
        EXPECT_TRUE(pushAwaiter.await_ready());
    };

    pushWait(6);
    pushWait(7);
    pushWait(8);

    auto pushAwaiter_9 = channel.PushWait(9);
    EXPECT_FALSE(pushAwaiter_9.await_ready());

    auto hdl_9 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(pushAwaiter_9.await_suspend(hdl_9));

    auto pushAwaiter_10 = channel.PushWait(10);
    EXPECT_FALSE(pushAwaiter_10.await_ready());

    auto hdl_10 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(pushAwaiter_10.await_suspend(hdl_10));

    popValue(6);
    popValue(7);
    popValue(8);
    popValue(9);
    popValue(10);

    EXPECT_TRUE(channel.Size() == 0);

    // Close the channel, before awaiter get's destroyed on the stack.
    // channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelTest_push_await_2)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(popAwaiter.await_suspend(hdl));

    auto pushAwaiter = channel.PushWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());

    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, popAwaiter.await_resume());
    EXPECT_EQ(val, 42);
}

TEST(BufferedChannelTest, BufferedChannelTest_emplace_await_2)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(popAwaiter.await_suspend(hdl));

    auto pushAwaiter = channel.PushWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());

    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, popAwaiter.await_resume());
    EXPECT_EQ(val, 42);
}

TEST(BufferedChannelTest, BufferedChannelTest_WaitForListeners_simple)
{
    tinycoro::BufferedChannel<int32_t> channel;

    auto listenersAwaiter = channel.WaitForListeners(2);
    EXPECT_FALSE(listenersAwaiter.await_ready());

    bool called{false};
    auto hdl = tinycoro::test::MakeCoroutineHdl([&called](auto) { called = true; });
    EXPECT_TRUE(listenersAwaiter.await_suspend(hdl));

    int32_t val;
    auto    popawaiter = channel.PopWait(val);
    EXPECT_FALSE(popawaiter.await_ready());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(popawaiter.await_suspend(hdl2));

    int32_t val2;
    auto    popawaiter2 = channel.PopWait(val2);
    EXPECT_FALSE(popawaiter2.await_ready());

    auto hdl3 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(popawaiter2.await_suspend(hdl3));

    // listenersAwaiter is notified
    EXPECT_TRUE(called);

    // Close the channel, before awaiter get's destroyed on the stack.
    channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelTest_push_before_WaitForListeners)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val;
    auto    popawaiter = channel.PopWait(val);
    EXPECT_FALSE(popawaiter.await_ready());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(popawaiter.await_suspend(hdl2));

    int32_t val2;
    auto    popawaiter2 = channel.PopWait(val2);
    EXPECT_FALSE(popawaiter2.await_ready());

    auto hdl3 = tinycoro::test::MakeCoroutineHdl();
    EXPECT_TRUE(popawaiter2.await_suspend(hdl3));

    auto listenersAwaiter = channel.WaitForListeners(2);
    EXPECT_TRUE(listenersAwaiter.await_ready());

    // Close the channel, before awaiter get's destroyed on the stack.
    channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelFunctionalTest)
{
    struct CloseChannelBuffer
    {
    };

    tinycoro::BufferedChannel<std::variant<int32_t, CloseChannelBuffer>> bufferedChannel;

    int32_t expected = 1;

    auto consumer = [&]() -> tinycoro::Task<void> {
        std::variant<int32_t, CloseChannelBuffer> val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await bufferedChannel.PopWait(val))
        {
            if (std::holds_alternative<int32_t>(val))
            {
                EXPECT_EQ(expected++, std::get<int32_t>(val));
            }
            else
            {
                // receive close entry
                bufferedChannel.Close();
            }
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        bufferedChannel.Push(1);
        bufferedChannel.Push(2);
        bufferedChannel.Push(3);
        bufferedChannel.Push(4);
        bufferedChannel.Push(CloseChannelBuffer{});
        co_return;
    };

    // single threaded scheduler
    tinycoro::Scheduler scheduler{1};

    tinycoro::AllOf(scheduler, consumer(), producer());
}

TEST(BufferedChannelTest, BufferedChannelFunctionalTest_pushWait_singleThreadedScheduler)
{
    struct CloseChannelBuffer
    {
    };
    
    int32_t expected = 1;

    tinycoro::BufferedChannel<std::variant<int32_t, CloseChannelBuffer>> bufferedChannel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        std::variant<int32_t, CloseChannelBuffer> val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await bufferedChannel.PopWait(val))
        {
            if (std::holds_alternative<int32_t>(val))
            {
                EXPECT_EQ(expected++, std::get<int32_t>(val));
            }
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await bufferedChannel.PushWait(1));
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await bufferedChannel.PushWait(2));
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await bufferedChannel.PushWait(3));
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await bufferedChannel.PushWait(4));
        EXPECT_EQ(tinycoro::EChannelOpStatus::LAST, co_await bufferedChannel.PushAndCloseWait(0));
        co_return;
    };

    // single threaded scheduler
    tinycoro::Scheduler scheduler{1};

    tinycoro::AllOf(scheduler, consumer(), producer());
}

struct BufferedChannelTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BufferedChannelTest, BufferedChannelTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(BufferedChannelTest, BufferedChannelTest_max_size)
{
    tinycoro::BufferedChannel<int32_t> channel{GetParam()};
    EXPECT_EQ(channel.MaxSize(), GetParam());
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_param)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                   latch{count};
    tinycoro::BufferedChannel<size_t> channel{count};

    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            // no lock needed here only one consumer
            auto [iter, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);

            latch.CountDown();
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            channel.Push(i);
        }

        // waiting for the latch
        co_await latch;

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::AllOf(scheduler, producer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_param_pushWait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                   latch{count};
    tinycoro::BufferedChannel<size_t> channel{count};

    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            // no lock needed here only one consumer
            auto [iter, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);

            latch.CountDown();
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await channel.PushWait(i));
        }

        // waiting for the latch
        co_await latch;

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::AllOf(scheduler, producer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_paramMulti)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                   latch{count};
    tinycoro::BufferedChannel<size_t> channel{count};

    std::mutex       mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            {
                // lock needed here multi consumer
                std::scoped_lock lock{mtx};
                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }

            latch.CountDown();
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            channel.Push(i);
        }

        // waiting for the latch
        co_await latch;

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer(), consumer());

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_paramMulti_pushWait)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                   latch{count};
    tinycoro::BufferedChannel<size_t> channel{count};

    std::mutex       mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            {
                // lock needed here multi consumer
                std::scoped_lock lock{mtx};
                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }

            latch.CountDown();
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await channel.PushWait(i));
        }

        // waiting for the latch
        co_await latch;

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer(), consumer());

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_waitForListeners)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::BufferedChannel<size_t> channel{count};

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t value{};
        auto   status = co_await channel.PopWait(value);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::CLOSED);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        co_await channel.WaitForListeners(count);
        channel.Close();
    };

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(producer());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
    }

    EXPECT_NO_THROW(tinycoro::AllOf(scheduler, std::move(tasks)));
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_waitForListeners_multi_waiters)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::BufferedChannel<size_t> channel{count};

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t value{};
        auto   status = co_await channel.PopWait(value);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::CLOSED);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        co_await channel.WaitForListeners(count);
        channel.Close();
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
        tasks.push_back(producer());
    }

    EXPECT_NO_THROW(tinycoro::AllOf(scheduler, std::move(tasks)));
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_waitForListenersClose)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::BufferedChannel<size_t> channel{count};

    auto consumer = [&]() -> tinycoro::Task<void> { co_await channel.WaitForListeners(count); };

    auto producer = [&]() -> tinycoro::Task<void> {
        // close the channel and wake up all awaiters
        channel.Close();
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
    }
    tasks.push_back(producer());

    EXPECT_NO_THROW(tinycoro::AllOf(scheduler, std::move(tasks)));
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_paramMulti_destructorClose)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::Latch latch{count};

    auto channel = std::make_unique<tinycoro::BufferedChannel<size_t>>(count);

    std::mutex       mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel->PopWait(val))
        {
            {
                // lock needed here multi consumer
                std::scoped_lock lock{mtx};
                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }

            latch.CountDown();
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            channel->Push(i);
        }

        // waiting for the latch
        co_await latch;

        // last element is done in channel.
        // Wait to have all the listeners up and running
        co_await channel->WaitForListeners(count);

        // closing the channel after latch is done with destructor
        channel.reset();
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
    }
    tasks.push_back(producer());

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_param_autoEvent)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::AutoEvent               event;
    tinycoro::BufferedChannel<size_t> channel{count};

    std::mutex       mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            {
                // lock needed here multi consumer
                std::scoped_lock lock{mtx};
                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }

            event.Set();
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            channel.Push(i);

            // waiting for the event
            co_await event;
        }

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer(), consumer());

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PushClose)
{
    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel{count};

    auto consumer = [&]() -> tinycoro::Task<void> {
        std::set<size_t> allValues;

        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto [iter, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);
        }

        EXPECT_EQ(allValues.size(), count);
    };

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    for (size_t i = 0; i < count; ++i)
    {
        if (i + 1 != count)
        {
            channel.Push(i);
        }
        else
        {
            channel.PushAndClose(i);
        }
    }

    tinycoro::AllOf(scheduler, consumer());
}

TEST(BufferedChannelTest, BufferedChannelTest_PushCloseMulti)
{
    tinycoro::SoftClock               clock;
    tinycoro::Scheduler               scheduler{1};
    tinycoro::BufferedChannel<size_t> channel;

    std::vector<size_t> allValues;
    tinycoro::Mutex     mutex;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        if (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto lock = co_await mutex;
            allValues.push_back(val);
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        co_await tinycoro::SleepFor(clock, 50ms);

        channel.Push(39u);
        channel.Push(40u);
        channel.Push(41u);
        channel.PushAndClose(42u);

        EXPECT_THROW(channel.Push(33u), tinycoro::BufferedChannelException);
    };

    tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer());

    EXPECT_EQ(allValues.size(), 4);

    EXPECT_EQ(allValues[0], 39);
    EXPECT_EQ(allValues[1], 40);
    EXPECT_EQ(allValues[2], 41);
    EXPECT_EQ(allValues[3], 42);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PushWait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::BufferedChannel<size_t> channel{10};

    auto task = [&](auto i) -> tinycoro::TaskNIC<int32_t> {
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PushWait(42u));
        co_return i;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        tasks.emplace_back(task(static_cast<int32_t>(it)));
    }

    auto result = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);

    size_t sizeCount{};
    for (size_t i = 1; i < result.size(); ++i)
    {
        if (result[i].has_value())
        {
            EXPECT_TRUE(sizeCount < channel.Size());
            sizeCount++;
        }
        else
        {
            EXPECT_FALSE(result[i].has_value());
        }
    }
}

TEST_P(BufferedChannelTest, BufferedChannelTest_Push_and_Pop_Wait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::BufferedChannel<size_t> channel{5};

    // check if all tasks are running
    // non_initial_cancellable tasks.
    std::atomic<size_t> cc{};

    auto producer = [&](auto i) -> tinycoro::TaskNIC<size_t> {
        cc++;
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PushWait(42u));
        co_return i;
    };

    auto consumer = [&]() -> tinycoro::TaskNIC<size_t> {
        size_t val;
        cc++;
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return val;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<size_t> {
        co_await tinycoro::SleepFor(clock, 50ms);
        co_return 44u;
    };

    std::vector<tinycoro::TaskNIC<size_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        tasks.emplace_back(producer(it));
        tasks.emplace_back(consumer());
    }

    auto result = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);
    EXPECT_EQ(cc, count * 2);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_Push_and_Pop_Wait_cancel_inline)
{
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::BufferedChannel<size_t> channel{5};

    // check if all tasks are running
    // non_initial_cancellable tasks.
    std::atomic<size_t> cc{};

    auto producer = [&](auto i) -> tinycoro::TaskNIC<size_t> {
        cc++;
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PushWait(42u));
        co_return i;
    };

    auto consumer = [&]() -> tinycoro::TaskNIC<size_t> {
        size_t val;
        cc++;
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return val;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<size_t> {
        co_await tinycoro::SleepFor(clock, 50ms);
        co_return 44u;
    };

    std::vector<tinycoro::TaskNIC<size_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        tasks.emplace_back(producer(it));
        tasks.emplace_back(consumer());
    }

    auto result = tinycoro::AnyOf(std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);
    EXPECT_EQ(cc, count * 2);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PopWait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::BufferedChannel<size_t> channel{10};

    auto task = [&]() -> tinycoro::TaskNIC<int32_t> {
        size_t                val{};
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto _ : std::views::iota(0u, count))
    {
        tasks.emplace_back(task());
    }

    auto result = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);

    for (size_t i = 1; i < result.size(); ++i)
    {
        EXPECT_FALSE(result[i].has_value());
    }
}

// STUCK HERE??
TEST_P(BufferedChannelTest, BufferedChannelTest_ListenerWait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::BufferedChannel<size_t> channel{10};

    auto task = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(channel.WaitForListeners(count + 1));
        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto _ : std::views::iota(0u, count))
    {
        tasks.emplace_back(task());
    }

    auto result = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);

    for (size_t i = 1; i < result.size(); ++i)
    {
        EXPECT_FALSE(result[i].has_value());
    }
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PushClose_multi)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel{count};
    std::mutex                        mtx;
    std::set<size_t>                  allValues;

    size_t lastValue{};

    auto consumer = [&]() -> tinycoro::Task<void> {
        while (true)
        {
            size_t val;
            auto   status = co_await channel.PopWait(val);

            if (status == tinycoro::EChannelOpStatus::CLOSED)
            {
                break;
            }

            if (status == tinycoro::EChannelOpStatus::LAST)
            {
                lastValue = val;
            }

            {
                std::scoped_lock lock{mtx};
                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            if (i + 1 != count)
            {
                channel.Push(i);
            }
            else
            {
                channel.PushAndClose(i);
            }
        }

        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
    }
    tasks.push_back(producer());

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(lastValue, count - 1);
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PushCloseWait_multi)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel{count};
    std::mutex                        mtx;
    std::set<size_t>                  allValues;

    size_t lastValue{};

    auto consumer = [&]() -> tinycoro::Task<void> {
        while (true)
        {
            size_t val;
            auto   status = co_await channel.PopWait(val);

            if (status == tinycoro::EChannelOpStatus::CLOSED)
            {
                break;
            }

            if (status == tinycoro::EChannelOpStatus::LAST)
            {
                lastValue = val;
            }

            {
                std::scoped_lock lock{mtx};
                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count - 1; ++i)
        {
            if (i + 1 != count)
            {
                EXPECT_TRUE(tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PushWait(i));
            }
        }

        EXPECT_TRUE(tinycoro::EChannelOpStatus::LAST == co_await channel.PushAndCloseWait(count - 1));
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
    }
    tasks.push_back(producer());

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(lastValue, count - 1);
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_WaitPush_singleThread_minQueueSize)
{
    tinycoro::Scheduler scheduler{1};

    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel{1};
    std::set<size_t>                  allValues;
    size_t                            currentValue{0};

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t last{0};
        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto [_, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);

            // increment order need to receive
            if (last != 0)
            {
                EXPECT_EQ(last + 1, val);
            }

            last = val;
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        while (currentValue < count)
        {
            ++currentValue;
            auto status = co_await channel.PushWait(currentValue);

            EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        }

        channel.Close();
    };

    tinycoro::AllOf(scheduler, consumer(), producer());

    EXPECT_EQ(currentValue, count);
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_pushWait_fixedQueueSize)
{
    const auto count = GetParam();

    constexpr size_t channelSize = 100;

    tinycoro::Scheduler               scheduler;
    tinycoro::BufferedChannel<size_t> channel{channelSize};

    std::mutex       mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            {
                // lock needed here multi consumer
                std::scoped_lock lock{mtx};

                EXPECT_TRUE(channel.Size() <= channelSize);

                auto [iter, inserted] = allValues.insert(val);
                EXPECT_TRUE(inserted);
            }
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count - 1; ++i)
        {
            EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, co_await channel.PushWait(i));
        }

        EXPECT_EQ(tinycoro::EChannelOpStatus::LAST, co_await channel.PushAndCloseWait(count - 1));
    };

    tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer(), consumer());

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_pushWait_close_fixedQueueSize)
{
    const auto count = GetParam();

    tinycoro::Scheduler               scheduler;
    tinycoro::BufferedChannel<size_t> channel{1};

    std::set<size_t> allValues;

    // push first value in the channel to make them full.
    channel.Push(41u);

    auto consumer = [&]() -> tinycoro::Task<void> {
        // channel should be full, so CLOSED is returned.
        EXPECT_EQ(tinycoro::EChannelOpStatus::CLOSED, co_await channel.PushWait(42u));
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        channel.Close();

        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(consumer());
    }
    tasks.push_back(producer());

    tinycoro::AllOf(scheduler, std::move(tasks));
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_tryPush)
{
    const auto count = GetParam();

    tinycoro::Scheduler               scheduler;
    tinycoro::BufferedChannel<size_t> channel{1};

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        size_t expected{0};
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            EXPECT_EQ(val, expected++);
        }

        EXPECT_EQ(count, expected - 1);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        size_t val{};
        while (val < count)
        {
            if (channel.TryPush(val))
            {
                val++;
            }
        }

        while (not channel.TryPushAndClose(count))
            ;

        // the channek is closed, this should return false
        EXPECT_FALSE(channel.TryPush(42u));

        co_return;
    };

    tinycoro::AllOf(scheduler, producer(), consumer());
}

struct BufferedChannelTimeoutTest : testing::TestWithParam<size_t>
{
    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler;
};

INSTANTIATE_TEST_SUITE_P(BufferedChannelTimeoutTest, BufferedChannelTimeoutTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(BufferedChannelTimeoutTest, BufferedChannelTimeoutTest_timeout_all_push_wait)
{
    auto count = GetParam();

    tinycoro::BufferedChannel<int32_t> channel{1};

    std::atomic<int32_t> done{0};
    std::atomic<int32_t> pushSuccess{0};

    auto task = [&]() -> tinycoro::TaskNIC<> {
        auto opt = co_await tinycoro::TimeoutAwait{clock, channel.PushWait(42), 10ms};

        if(opt.has_value())
        {
            pushSuccess++;
        }

        done++;
    };

    std::vector<decltype(task())> tasks;
    tasks.reserve(count);
    for(size_t i = 0; i < count; i++)
    {
        tasks.emplace_back(task());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, done);

    // only the first awaiter can push...
    EXPECT_EQ(pushSuccess, 1);
}

TEST_P(BufferedChannelTimeoutTest, BufferedChannelTimeoutTest_timeout_race_push_pop_wait)
{
    auto count = GetParam();

    tinycoro::BufferedChannel<int32_t> channel{1};

    std::atomic<int32_t> popDone{0};
    std::atomic<int32_t> pushDone{0};

    auto producer = [&]() -> tinycoro::TaskNIC<> {
        for(size_t i = 0; i < count; ++i)
        {
            co_await tinycoro::TimeoutAwait{clock, channel.PushWait(42), 1ms};
            pushDone++;
        }
    };

    auto consumer = [&]() -> tinycoro::TaskNIC<> {
        for(size_t i = 0; i < count; ++i)
        {
            int32_t val;
            co_await tinycoro::TimeoutAwait{clock, channel.PopWait(val), 1ms};
            popDone++;
        }
    };

    tinycoro::AllOf(scheduler, consumer(), producer());

    EXPECT_EQ(count, popDone);
    EXPECT_EQ(count, pushDone);
}

TEST_P(BufferedChannelTimeoutTest, BufferedChannelTimeoutTest_timeout_race_listeners_wait)
{
    auto count = GetParam();

    tinycoro::BufferedChannel<int32_t> channel{1};

    std::atomic<int32_t> listenerDone{0};

    std::atomic_flag flag;

    auto listener = [&]() -> tinycoro::TaskNIC<> {
        for(size_t i = 0; i < count; ++i)
        {
            co_await tinycoro::TimeoutAwait{clock, channel.WaitForListeners(1), 1ms};
            listenerDone++;
        }

        flag.test_and_set();
    };

    auto consumer = [&]() -> tinycoro::TaskNIC<> {
        while(flag.test() == false)
        {
            std::this_thread::sleep_for(40ms);
            int32_t val;
            co_await tinycoro::TimeoutAwait{clock, channel.PopWait(val), 1ms};
        }
    };

    tinycoro::AllOf(scheduler, listener(), consumer());

    EXPECT_EQ(count, listenerDone);
}

TEST_P(BufferedChannelTimeoutTest, BufferedChannelTimeoutTest_timeout_all_pop_wait)
{
    auto count = GetParam();

    tinycoro::BufferedChannel<int32_t> channel{1};

    std::atomic<int32_t> done{0};

    auto task = [&]() -> tinycoro::TaskNIC<> {
        int32_t val;
        auto opt = co_await tinycoro::TimeoutAwait{clock, channel.PopWait(val), 10ms};
        EXPECT_FALSE(opt.has_value());
        done++;
    };

    std::vector<decltype(task())> tasks;
    tasks.reserve(count);
    for(size_t i=0; i < count; i++)
    {
        tasks.emplace_back(task());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, done);
}

TEST_P(BufferedChannelTimeoutTest, BufferedChannelTimeoutTest_timeout_all_listeners_wait)
{
    auto count = GetParam();

    tinycoro::BufferedChannel<int32_t> channel{1};

    std::atomic<int32_t> done{0};

    auto task = [&]() -> tinycoro::TaskNIC<> {
        auto opt = co_await tinycoro::TimeoutAwait{clock, channel.WaitForListeners(1), 10ms};
        EXPECT_FALSE(opt.has_value());
        done++;
    };

    std::vector<decltype(task())> tasks;
    tasks.reserve(count);
    for(size_t i=0; i < count; i++)
    {
        tasks.emplace_back(task());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, done);
}