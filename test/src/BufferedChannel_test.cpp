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