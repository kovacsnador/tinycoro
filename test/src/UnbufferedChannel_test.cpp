#include <gtest/gtest.h>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(UnbufferedChannelTest, UnbufferedChannelTest_open)
{
    tinycoro::UnbufferedChannel<size_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Close();
    EXPECT_FALSE(channel.IsOpen());
}

template <typename, typename, typename>
class PopAwaiterMock
{
public:
    PopAwaiterMock(auto&, auto...) { }

    void Notify() const noexcept { };

    PopAwaiterMock* next{nullptr};
};

template <typename, typename, typename>
class PushAwaiterMock
{
public:
    PushAwaiterMock(auto&, auto...) { }

    void Notify() const noexcept { };

    PushAwaiterMock* next{nullptr};
};

template <typename, typename>
class ListenerAwaiterMock
{
public:
    ListenerAwaiterMock(auto&, auto...) { }

    void Notify() const noexcept { };

    ListenerAwaiterMock* next{nullptr};

    size_t value() { return 42; }
};

TEST(UnbufferedChannelTest, UnbufferedChannelTest_PopWait_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock, ListenerAwaiterMock> channel;

    int32_t val;
    auto    awaiter = channel.PopWait(val);

    using expectedAwaiterType = PopAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannel_PushWait_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock, ListenerAwaiterMock> channel;

    auto awaiter = channel.PushWait(1);

    using expectedAwaiterType = PushAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannel_PushWaitAndClose_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock, ListenerAwaiterMock> channel;

    auto awaiter = channel.PushAndCloseWait(1);

    using expectedAwaiterType = PushAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannel_WaitForListeners_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock, ListenerAwaiterMock> channel;

    auto awaiter = channel.WaitForListeners(1);

    using expectedAwaiterType = ListenerAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_open_push)
{
    tinycoro::UnbufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    auto pushAwaiter1 = channel.PushWait(42);
    EXPECT_TRUE(channel.IsOpen());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(pushAwaiter1.await_suspend(hdl1));

    auto pushAwaiter2 = channel.PushAndCloseWait(44);
    EXPECT_TRUE(channel.IsOpen());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(pushAwaiter2.await_suspend(hdl2));

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

TEST(UnbufferedChannelTest, UnbufferedChannelTest_push_await_ready)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(popAwaiter.await_suspend(hdl1));

    int32_t val2;
    auto    popAwaiter2 = channel.PopWait(val2);
    EXPECT_FALSE(popAwaiter2.await_ready());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(popAwaiter2.await_suspend(hdl2));

    auto pushAwaiter1 = channel.PushWait(42);
    EXPECT_TRUE(pushAwaiter1.await_ready());

    EXPECT_EQ(val, 42);

    auto pushAwaiter2 = channel.PushWait(44);
    EXPECT_TRUE(pushAwaiter2.await_ready());

    EXPECT_EQ(val2, 44);
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_push_await_resume)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(popAwaiter.await_suspend(hdl1));

    auto pushAwaiter = channel.PushWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());

    EXPECT_EQ(val, 42);
    EXPECT_EQ(popAwaiter.await_resume(), tinycoro::EChannelOpStatus::SUCCESS);
    EXPECT_EQ(pushAwaiter.await_resume(), tinycoro::EChannelOpStatus::SUCCESS);
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_push_await_resume_last)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(popAwaiter.await_suspend(hdl1));

    auto pushAwaiter = channel.PushAndCloseWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());

    EXPECT_EQ(val, 42);
    EXPECT_EQ(popAwaiter.await_resume(), tinycoro::EChannelOpStatus::LAST);
    EXPECT_EQ(pushAwaiter.await_resume(), tinycoro::EChannelOpStatus::LAST);
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_push_await_resume_close)
{
    tinycoro::UnbufferedChannel<int32_t> channel;

    channel.Close();

    int32_t val;
    auto    popAwaiter = channel.PopWait(val);
    EXPECT_TRUE(popAwaiter.await_ready());

    auto pushAwaiter = channel.PushAndCloseWait(42);
    EXPECT_TRUE(pushAwaiter.await_ready());

    EXPECT_EQ(popAwaiter.await_resume(), tinycoro::EChannelOpStatus::CLOSED);
    EXPECT_EQ(pushAwaiter.await_resume(), tinycoro::EChannelOpStatus::CLOSED);
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_close)
{
    tinycoro::UnbufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Close();
    EXPECT_FALSE(channel.IsOpen());
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_WaitForListeners_await_suspend)
{
    tinycoro::UnbufferedChannel<size_t> channel;

    size_t val;
    auto   popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(popAwaiter.await_suspend(hdl1));

    // testing the listener awaiter
    auto listenerAwaiter = channel.WaitForListeners(1);

    // calling directly await_suspend without await_ready
    EXPECT_FALSE(listenerAwaiter.await_suspend(tinycoro::test::MakeCoroutineHdl()));
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_WaitForListeners_await_ready_closed)
{
    tinycoro::UnbufferedChannel<size_t> channel;

    size_t val;
    auto   popAwaiter = channel.PopWait(val);
    EXPECT_FALSE(popAwaiter.await_ready());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(popAwaiter.await_suspend(hdl1));

    // close the channel
    channel.Close();

    // testing the listener awaiter
    auto listenerAwaiter = channel.WaitForListeners(1);

    // calling directly await_suspend without await_ready
    EXPECT_TRUE(listenerAwaiter.await_ready());
}

TEST(UnbufferedChannelTest, UnbufferedChannelFunctionalTest_simple)
{
    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        auto   status = co_await channel.PopWait(val);

        EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        EXPECT_EQ(val, 42);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        auto status = co_await channel.PushWait(42u);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST(UnbufferedChannelTest, UnbufferedChannelFunctionalTest_simple_push)
{
    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        auto   status = co_await channel.PopWait(val);

        EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        EXPECT_EQ(val, 42);
    };

    // starting a consumer in an async environment
    auto future = scheduler.Enqueue(consumer());

    auto result = channel.Push(42u);
    EXPECT_EQ(result, tinycoro::EChannelOpStatus::SUCCESS);

    // wait for the future
    future.get();
}

TEST(UnbufferedChannelTest, UnbufferedChannelFunctionalTest_simple_pop_before_push)
{
    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        auto   status = co_await channel.PopWait(val);

        EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        EXPECT_EQ(val, 42);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        // wait for the consumer
        co_await channel.WaitForListeners(1);

        // Push (the non coroutine version)
        auto result = channel.Push(42u);
        EXPECT_EQ(result, tinycoro::EChannelOpStatus::SUCCESS);
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST(UnbufferedChannelTest, UnbufferedChannelFunctionalTest_simple_push_and_close)
{
    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        auto   status = co_await channel.PopWait(val);

        EXPECT_EQ(status, tinycoro::EChannelOpStatus::LAST);
        EXPECT_EQ(val, 42);
    };

    // starting a consumer in an async environment
    auto future = scheduler.Enqueue(consumer());

    auto result = channel.PushAndClose(42u);
    EXPECT_EQ(result, tinycoro::EChannelOpStatus::LAST);

    // wait for the future
    future.get();
}

TEST(UnbufferedChannelTest, UnbufferedChannelFunctionalTest_lastElement)
{
    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        auto   status = co_await channel.PopWait(val);

        EXPECT_EQ(status, tinycoro::EChannelOpStatus::LAST);
        EXPECT_EQ(val, 42);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        auto status = co_await channel.PushAndCloseWait(42u);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::LAST);
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST(UnbufferedChannelTest, UnbufferedChannelFunctionalTest_close)
{
    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        auto   status = co_await channel.PopWait(val);

        EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        EXPECT_EQ(val, 42);

        channel.Close();

        status = co_await channel.PopWait(val);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::CLOSED);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        auto status = co_await channel.PushWait(42u);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);

        status = co_await channel.PushAndCloseWait(44u);
        EXPECT_EQ(status, tinycoro::EChannelOpStatus::CLOSED);
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_cleanup_callback_pushWait)
{
    std::vector<size_t> coll;
    auto                cleanup = [&coll](auto& val) { coll.push_back(val); };

    tinycoro::Latch                     latch{2};
    tinycoro::UnbufferedChannel<size_t> channel{cleanup};

    auto producer = [&](size_t val) -> tinycoro::Task<> { std::ignore = co_await channel.PushWait(val); };

    auto consumer = [&](size_t expected) -> tinycoro::Task<> {
        size_t val;
        std::ignore = co_await channel.PopWait(val);
        EXPECT_EQ(val, expected);

        latch.CountDown();
    };

    auto closer = [&]() -> tinycoro::Task<> {
        co_await latch;
        channel.Close();
    };

    tinycoro::RunInline(producer(40), producer(41), producer(42), producer(43), producer(44), consumer(40), consumer(41), closer());

    EXPECT_EQ(coll.size(), 3);

    size_t expected = 42;
    for (const auto& it : coll)
    {
        EXPECT_EQ(expected++, it);
    }
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_pop_awaiter_listener)
{
    tinycoro::UnbufferedChannel<size_t> channel;

    auto waitForListeners = [&]() -> tinycoro::Task<> { co_await channel.WaitForListeners(3); };

    auto producer = [&](size_t val) -> tinycoro::Task<> { std::ignore = co_await channel.PushWait(val); };

    auto consumer = [&](size_t expected) -> tinycoro::Task<> {
        size_t val;
        std::ignore = co_await channel.PopWait(val);
        EXPECT_EQ(val, expected);
    };

    tinycoro::RunInline(waitForListeners(), consumer(40), consumer(41), consumer(42), producer(40), producer(41), producer(42));
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_cancel)
{
    tinycoro::Scheduler                  scheduler;
    tinycoro::SoftClock                  clock;
    tinycoro::UnbufferedChannel<int32_t> channel;

    auto listener = [&]() -> tinycoro::Task<int32_t> {
        int32_t val;
        co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return 42;
    };

    auto listenerWaiter = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(channel.WaitForListeners(10)); };

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

TEST(UnbufferedChannelTest, UnbufferedChannelTest_cancel_inline)
{
    tinycoro::SoftClock                  clock;
    tinycoro::UnbufferedChannel<int32_t> channel;

    auto listener = [&]() -> tinycoro::Task<int32_t> {
        int32_t val;
        co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return 42;
    };

    auto listenerWaiter = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(channel.WaitForListeners(10)); };

    auto [r1, r2, r3, r4, r5, r6, r7, r8, r9] = tinycoro::AnyOfInline(listener(),
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

struct UnbufferedChannelTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(UnbufferedChannelTest, UnbufferedChannelTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_param)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::UnbufferedChannel<size_t> channel;

    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            // no lock needed here only one consumer
            auto [iter, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            auto status = co_await channel.PushWait(i);
            EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        }

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::GetAll(scheduler, producer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_paramMulti)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::UnbufferedChannel<size_t> channel;

    std::mutex       mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            std::scoped_lock lock{mtx};
            auto [iter, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            auto status = co_await channel.PushWait(i);
            EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
        }

        // closing the channel after latch is done
        channel.Close();
    };

    tinycoro::GetAll(scheduler, producer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_PushAndClose)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

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

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            if (i + 1 != count)
            {
                auto status = co_await channel.PushWait(i);
                EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
            }
            else
            {
                auto status = co_await channel.PushAndCloseWait(i);
                EXPECT_EQ(status, tinycoro::EChannelOpStatus::LAST);
            }
        }
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_PushAndClose_multi)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

    tinycoro::Mutex  mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto lock             = co_await mtx;
            auto [iter, inserted] = allValues.insert(val);
            lock.unlock();
            EXPECT_TRUE(inserted);
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            if (i + 1 != count)
            {
                auto status = co_await channel.PushWait(i);
                EXPECT_EQ(status, tinycoro::EChannelOpStatus::SUCCESS);
            }
            else
            {
                auto status = co_await channel.PushAndCloseWait(i);
                EXPECT_EQ(status, tinycoro::EChannelOpStatus::LAST);
            }
        }
    };

    tinycoro::GetAll(scheduler, consumer(), producer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_push_pop_close)
{
    tinycoro::SoftClock clock;
    const auto          count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

    tinycoro::Mutex  mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto lock             = co_await mtx;
            auto [iter, inserted] = allValues.insert(val);
            lock.unlock();
            EXPECT_TRUE(inserted);
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        std::remove_const_t<decltype(count)> i{};
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PushWait(i))
        {
            ++i;
        }
    };

    auto closer = [&]() -> tinycoro::Task<void> {
        co_await tinycoro::SleepFor(clock, 200ms);
        channel.Close();
    };

    tinycoro::GetAll(scheduler, consumer(), producer(), consumer(), consumer(), consumer(), consumer(), closer());

    // check for values
    bool found{true};
    for (std::remove_const_t<decltype(count)> i{}; i < count; ++i)
    {
        auto f = allValues.contains(i);
        ASSERT_TRUE((found == false && f == false) || found);
        found = f;
    }
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_simple_push)
{
    const auto param = GetParam();

    tinycoro::Scheduler                 scheduler{2};
    tinycoro::UnbufferedChannel<size_t> channel;

    size_t controlCount{0};
    size_t controlCount2{0};

    auto consumer = [&]() -> tinycoro::Task<void> {
        //size_t controlCount{0};
        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            EXPECT_EQ(controlCount++, val);
        }

        EXPECT_EQ(controlCount, param);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < param; ++i)
        {
            // just a normal blocking push
            channel.Push(i);
            controlCount2++;
        }
        channel.Close();

        co_return;
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_simple_pushAndClose)
{
    const auto param = GetParam();

    tinycoro::Scheduler                 scheduler;
    tinycoro::UnbufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t controlCount{0};
        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            EXPECT_EQ(controlCount++, val);
        }

        EXPECT_EQ(controlCount, param);
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < param - 1; ++i)
        {
            // just a normal blocking push
            channel.Push(i);
        }
        // normal blocking push and close
        channel.PushAndClose(param - 1);

        co_return;
    };

    tinycoro::GetAll(scheduler, consumer(), producer());
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_waitForListeners)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

    auto listeners = [&]() -> tinycoro::Task<void> {
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
        tasks.push_back(listeners());
    }

    EXPECT_NO_THROW(tinycoro::GetAll(scheduler, tasks));
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_waitForListeners_multi_waiters)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

    auto listeners = [&]() -> tinycoro::Task<void> {
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
        tasks.push_back(listeners());
        tasks.push_back(producer());
    }

    EXPECT_NO_THROW(tinycoro::GetAll(scheduler, tasks));
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_waitForListenersClose)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

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

    EXPECT_NO_THROW(tinycoro::GetAll(scheduler, tasks));
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_PushWait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                          count = GetParam();
    tinycoro::UnbufferedChannel<size_t> channel;

    auto task = [&]() -> tinycoro::Task<int32_t> {
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PushWait(42u));
        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
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

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_PopWait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                          count = GetParam();
    tinycoro::UnbufferedChannel<size_t> channel;

    auto task = [&]() -> tinycoro::Task<int32_t> {
        size_t                val{};
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
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

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_Push_and_Pop_Wait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::UnbufferedChannel<size_t> channel;

    auto producer = [&](auto i) -> tinycoro::Task<size_t> {
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PushWait(42u));
        co_return i;
    };

    auto consumer = [&]() -> tinycoro::Task<size_t> {
        size_t val;
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return val;
    };

    auto sleep = [&]() -> tinycoro::Task<size_t> {
        co_await tinycoro::SleepFor(clock, 50ms);
        co_return 44u;
    };

    std::vector<tinycoro::Task<size_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        tasks.emplace_back(producer(it));
        tasks.emplace_back(consumer());
    }

    auto result = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);
}

TEST_P(UnbufferedChannelTest,UnbufferedChannelTest_Push_and_Pop_Wait_cancel_inline)
{
    tinycoro::SoftClock clock;

    const auto                        count = GetParam();
    tinycoro::UnbufferedChannel<size_t> channel;

    auto producer = [&](auto i) -> tinycoro::Task<size_t> {
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PushWait(42u));
        co_return i;
    };

    auto consumer = [&]() -> tinycoro::Task<size_t> {
        size_t val;
        [[maybe_unused]] auto state = co_await tinycoro::Cancellable(channel.PopWait(val));
        co_return val;
    };

    auto sleep = [&]() -> tinycoro::Task<size_t> {
        co_await tinycoro::SleepFor(clock, 50ms);
        co_return 44u;
    };

    std::vector<tinycoro::Task<size_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        tasks.emplace_back(producer(it));
        tasks.emplace_back(consumer());
    }

    auto result = tinycoro::AnyOfInline(std::move(tasks));

    EXPECT_EQ(result[0].value(), 44);
}

TEST_P(UnbufferedChannelTest, UnbufferedChannelTest_ListenerWait_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    const auto                          count = GetParam();
    tinycoro::UnbufferedChannel<size_t> channel;

    auto task = [&]() -> tinycoro::Task<int32_t> { co_await tinycoro::Cancellable(channel.WaitForListeners(count + 1)); co_return 42; };

    auto sleep = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
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

// TODO listener awaiter for closing need to be tested....