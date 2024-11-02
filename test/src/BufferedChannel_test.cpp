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

TEST(BufferedChannelTest, BufferedChannelTest_moveOnlyValue)
{
    struct MoveOnly
    {
        MoveOnly() = default;

        MoveOnly(int32_t v)
        : value{v}
        {
        }

        MoveOnly(MoveOnly&&) noexcept            = default;
        MoveOnly& operator=(MoveOnly&&) noexcept = default;

        int32_t value{};
    };

    tinycoro::BufferedChannel<MoveOnly> channel;
    EXPECT_TRUE(channel.Empty());
    channel.Push(42);
    EXPECT_FALSE(channel.Empty());

    MoveOnly val;
    auto     awaiter = channel.PopWait(val);
    EXPECT_TRUE(awaiter.await_ready());

    EXPECT_TRUE(channel.Empty());

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::BufferedChannel_OpStatus::SUCCESS, result);
    EXPECT_EQ(42, val.value);
}

template <typename, typename, typename>
struct AwaiterMock
{
    AwaiterMock(auto&, auto, auto) { }

    void Notify() const noexcept {};

    AwaiterMock* next{nullptr};
};

TEST(BufferedChannelTest, BufferedChannelTest_coawaitReturn)
{
    tinycoro::detail::BufferedChannel<int32_t, AwaiterMock, std::queue> channel;

    int32_t val;
    auto    awaiter = channel.PopWait(val);

    using expectedAwaiterType = AwaiterMock<decltype(channel), tinycoro::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BufferedChannelTest, BufferedChannelTest_await_ready)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(val, 0);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_suspend)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    bool pauseResumeCalled{false};
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());
    EXPECT_EQ(val, 0);

    channel.Push(42);
    EXPECT_TRUE(pauseResumeCalled);
    EXPECT_EQ(val, 42);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    bool pauseResumeCalled{false};
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());
    EXPECT_EQ(val, 0);

    channel.Push(42);

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::BufferedChannel_OpStatus::SUCCESS, result);
    EXPECT_EQ(val, 42);
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    bool pauseResumeCalled{false};
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

    channel.Push(42);

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);
    EXPECT_EQ(val, 42);

    channel.Close();

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::BufferedChannel_OpStatus::CLOSED, result);
    EXPECT_EQ(val, 42);
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
        auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

        EXPECT_EQ(awaiter.await_suspend(hdl), hdl);

        auto result = awaiter.await_resume();
        EXPECT_EQ(tinycoro::BufferedChannel_OpStatus::SUCCESS, result);
        EXPECT_EQ(val, i);
    }
}

TEST(BufferedChannelTest, BufferedChannelFunctionalTest)
{
    struct CloseChannelBuffer
    {
    };

    tinycoro::BufferedChannel<std::variant<int32_t, CloseChannelBuffer>> bufferedChannel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        std::variant<int32_t, CloseChannelBuffer> val;
        while (tinycoro::BufferedChannel_OpStatus::SUCCESS == co_await bufferedChannel.PopWait(val))
        {
            if (std::holds_alternative<int32_t>(val))
            {
                static int32_t expected = 1;
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

    tinycoro::GetAll(scheduler, consumer(), producer());
}

struct BufferedChannelTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BufferedChannelTest,
                         BufferedChannelTest,
                         testing::Values(10,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,                                  
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         1,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500,
                                         10,
                                         100,
                                         100,
                                         100,
                                         500));

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_param)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                    latch{count};
    std::atomic<size_t> counter{};
    tinycoro::BufferedChannel<int32_t> channel;

    std::set<int32_t> allValues;

    auto consumer = [&]() -> tinycoro::Task<void> {
        int32_t val;
        while (tinycoro::BufferedChannel_OpStatus::SUCCESS == co_await channel.PopWait(val))
        {
            // no lock needed here only one consumer
            auto [iter, inserted] = allValues.insert(val);
            EXPECT_TRUE(inserted);

            //latch.CountDown();
            counter++;
        }
    };

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            channel.Push(i);
        }

        // waiting for the latch
        //co_await latch;
        while(counter < count){}

        // closing the channel after latch is done
        channel.Close();
        co_return;
    };

    tinycoro::GetAll(scheduler, producer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}