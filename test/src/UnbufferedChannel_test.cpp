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
    PopAwaiterMock(auto...) { }

    void Notify() const noexcept {};

    PopAwaiterMock* next{nullptr};
};

template <typename, typename, typename>
class PushAwaiterMock
{
public:
    PushAwaiterMock(auto...) { }

    void Notify() const noexcept {};

    PushAwaiterMock* next{nullptr};
};

TEST(UnbufferedChannelTest, UnbufferedChannelTest_PopWait_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock> channel;

    int32_t val;
    auto    awaiter = channel.PopWait(val);

    using expectedAwaiterType = PopAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannel_PushWait_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock> channel;

    auto awaiter = channel.PushWait(1);

    using expectedAwaiterType = PushAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannel_PushWaitAndClose_return)
{
    tinycoro::detail::UnbufferedChannel<int32_t, PopAwaiterMock, PushAwaiterMock> channel;

    auto awaiter = channel.PushAndCloseWait(1);

    using expectedAwaiterType = PushAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(UnbufferedChannelTest, UnbufferedChannelTest_open_push)
{
    tinycoro::UnbufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    auto pushAwaiter1 = channel.PushWait(42);
    EXPECT_TRUE(channel.IsOpen());

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] {});
    EXPECT_TRUE(pushAwaiter1.await_suspend(hdl1));

    auto pushAwaiter2 = channel.PushAndCloseWait(44);
    EXPECT_TRUE(channel.IsOpen());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([] {});
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

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] {});
    EXPECT_TRUE(popAwaiter.await_suspend(hdl1));

    int32_t val2;
    auto    popAwaiter2 = channel.PopWait(val2);
    EXPECT_FALSE(popAwaiter2.await_ready());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([] {});
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

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] {});
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

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] {});
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

    tinycoro::Mutex mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {

        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto lock = co_await mtx;
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
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::UnbufferedChannel<size_t> channel;

    tinycoro::Mutex mtx;
    std::set<size_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {

        size_t val;
        while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto lock = co_await mtx;
            auto [iter, inserted] = allValues.insert(val);
            lock.unlock();
            EXPECT_TRUE(inserted);
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {

        std::remove_const_t<decltype(count)> i{};
        while(tinycoro::EChannelOpStatus::CLOSED != co_await channel.PushWait(i))
        {
            ++i;
        }
    };

    auto closer = [&]() ->tinycoro::Task<void>
    {
        co_await tinycoro::Sleep(200ms);
        channel.Close();
    };

    tinycoro::GetAll(scheduler, consumer(), producer(), consumer(), consumer(), consumer(), consumer(), closer());
    
    // check for values
    bool found{true};
    for(std::remove_const_t<decltype(count)> i{}; i < count; ++i)
    {
        auto f = allValues.contains(i);
        ASSERT_TRUE((found == false && f == false) || found);
        found = f;
    }
}