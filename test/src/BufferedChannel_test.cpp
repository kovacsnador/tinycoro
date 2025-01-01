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

TEST(BufferedChannelTest, BufferedChannelTest_open_emplace)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Emplace(42);
    EXPECT_TRUE(channel.IsOpen());

    channel.EmplaceAndClose(44);
    EXPECT_TRUE(channel.IsOpen());

    int32_t val;
    auto    awaiter1 = channel.PopWait(val);

    EXPECT_TRUE(awaiter1.await_ready());
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

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([] {});

    EXPECT_EQ(awaiter1.await_suspend(hdl1), hdl1);
    EXPECT_TRUE(channel.IsOpen());
    EXPECT_EQ(val, 42);

    auto awaiter2 = channel.PopWait(val);

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([] {});

    EXPECT_EQ(awaiter2.await_suspend(hdl2), hdl2);
    EXPECT_FALSE(channel.IsOpen()); // channel need to be closed

    EXPECT_THROW(channel.Push(33), tinycoro::BufferedChannelException);

    EXPECT_EQ(val, 44);
}

TEST(BufferedChannelTest, BufferedChannelTest_open_emplace_await_suspend)
{
    tinycoro::BufferedChannel<int32_t> channel;
    EXPECT_TRUE(channel.IsOpen());

    channel.Emplace(42);
    EXPECT_TRUE(channel.IsOpen());

    channel.EmplaceAndClose(44);
    EXPECT_TRUE(channel.IsOpen());

    int32_t val;
    auto    awaiter1 = channel.PopWait(val);

    auto hdl = tinycoro::test::MakeCoroutineHdl([] {});

    EXPECT_EQ(awaiter1.await_suspend(hdl), hdl);
    EXPECT_TRUE(channel.IsOpen());
    EXPECT_EQ(val, 42);

    auto awaiter2 = channel.PopWait(val);

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([] {});

    EXPECT_EQ(awaiter2.await_suspend(hdl2), hdl2);
    EXPECT_FALSE(channel.IsOpen()); // channel need to be closed

    EXPECT_THROW(channel.EmplaceAndClose(33), tinycoro::BufferedChannelException);

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

    EXPECT_TRUE(channel.Empty());

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
    EXPECT_EQ(42, val.value);
}

template <typename, typename, typename>
class PopAwaiterMock
{
public:
    PopAwaiterMock(auto, auto, auto) { }

    void Notify() const noexcept {};

    PopAwaiterMock* next{nullptr};
};

template <typename C, typename E>
class ListenerAwaiterMock
{
public:
    ListenerAwaiterMock(C&, E, size_t) { }

    void Notify() const noexcept {};

    ListenerAwaiterMock* next{nullptr};
};

TEST(BufferedChannelTest, BufferedChannelTest_coawaitReturn)
{
    tinycoro::detail::BufferedChannel<int32_t, PopAwaiterMock, ListenerAwaiterMock, tinycoro::detail::Queue> channel;

    int32_t val;
    auto    awaiter = channel.PopWait(val);

    using expectedAwaiterType = PopAwaiterMock<decltype(channel), tinycoro::detail::PauseCallbackEvent, int32_t>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BufferedChannelTest, BufferedChannelTest_coawait_listenerWaiter)
{
    tinycoro::detail::BufferedChannel<int32_t, PopAwaiterMock, ListenerAwaiterMock, tinycoro::detail::Queue> channel;

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

using ListenerTestData = std::tuple<size_t, bool>;

struct BufferedChannelListenerTest : testing::TestWithParam<ListenerTestData>
{
};

INSTANTIATE_TEST_SUITE_P(
    BufferedChannelListenerTest,
    BufferedChannelListenerTest,
    testing::Values(ListenerTestData{1, true}, ListenerTestData{2, true}, ListenerTestData{3, true}, ListenerTestData{4, false}, ListenerTestData{10, false})
);

TEST_P(BufferedChannelListenerTest, BufferedChannelTest_await_ready_with_listener)
{
    auto [count, ready] = GetParam();

    tinycoro::BufferedChannel<int32_t> channel;

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([]{});
    int32_t val1{};
    auto awaiter1 = channel.PopWait(val1);
    EXPECT_EQ(awaiter1.await_suspend(hdl1), std::noop_coroutine());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([]{});
    int32_t val2{};
    auto awaiter2 = channel.PopWait(val2);
    EXPECT_EQ(awaiter2.await_suspend(hdl2), std::noop_coroutine());

    auto hdl3 = tinycoro::test::MakeCoroutineHdl([]{});
    int32_t val3{};
    auto awaiter3 = channel.PopWait(val3);
    EXPECT_EQ(awaiter3.await_suspend(hdl3), std::noop_coroutine());

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

    auto hdl1 = tinycoro::test::MakeCoroutineHdl([]{});
    int32_t val1{};
    auto awaiter1 = channel.PopWait(val1);
    EXPECT_EQ(awaiter1.await_suspend(hdl1), std::noop_coroutine());

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([]{});
    int32_t val2{};
    auto awaiter2 = channel.PopWait(val2);
    EXPECT_EQ(awaiter2.await_suspend(hdl2), std::noop_coroutine());

    auto hdl3 = tinycoro::test::MakeCoroutineHdl([]{});
    int32_t val3{};
    auto awaiter3 = channel.PopWait(val3);
    EXPECT_EQ(awaiter3.await_suspend(hdl3), std::noop_coroutine());

    auto listenerAwaiter = channel.WaitForListeners(count);

    auto listenerHdl = tinycoro::test::MakeCoroutineHdl([]{});

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
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());
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
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());
    EXPECT_EQ(val, 0);

    channel.Push(42);

    auto result = awaiter.await_resume();
    EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
    EXPECT_EQ(val, 42);

    // because of the awaiters registration close is necessary here.
    channel.Close();
}

TEST(BufferedChannelTest, BufferedChannelTest_await_resume_push_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    auto hdl = tinycoro::test::MakeCoroutineHdl([] {});

    channel.Push(42);

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);
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

TEST(BufferedChannelTest, BufferedChannelTest_await_resume_close)
{
    tinycoro::BufferedChannel<int32_t> channel;

    int32_t val{};
    auto    awaiter = channel.PopWait(val);

    auto hdl = tinycoro::test::MakeCoroutineHdl([] {});

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);

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
        auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseResumeCalled] { pauseResumeCalled = true; });

        EXPECT_EQ(awaiter.await_suspend(hdl), hdl);

        auto result = awaiter.await_resume();
        EXPECT_EQ(tinycoro::EChannelOpStatus::SUCCESS, result);
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
        while (tinycoro::EChannelOpStatus::SUCCESS == co_await bufferedChannel.PopWait(val))
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
                         testing::Values(1,
                                         10,
                                         100,
                                         1000,
                                         10000));

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_param)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                   latch{count};
    tinycoro::BufferedChannel<size_t> channel;

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

    tinycoro::GetAll(scheduler, producer(), consumer());
    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_paramMulti)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::Latch                   latch{count};
    tinycoro::BufferedChannel<size_t> channel;

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

    tinycoro::GetAll(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer(), consumer());

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_waitForListeners)
{
    const auto count = GetParam();
    tinycoro::Scheduler scheduler;

    tinycoro::BufferedChannel<size_t> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t value{};
        auto status = co_await channel.PopWait(value);
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

    EXPECT_NO_THROW(tinycoro::GetAll(scheduler, tasks));
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_paramMulti_destructorClose)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::Latch latch{count};

    auto channel = std::make_unique<tinycoro::BufferedChannel<size_t>>();

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

    tinycoro::GetAll(scheduler, tasks);

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelFunctionalTest_param_autoEvent)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{8};

    tinycoro::AutoEvent               event;
    tinycoro::BufferedChannel<size_t> channel;

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

    tinycoro::GetAll(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer(), consumer());

    EXPECT_EQ(allValues.size(), count);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PushClose)
{
    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel;

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

    tinycoro::GetAll(scheduler, consumer());
}

TEST(BufferedChannelTest, BufferedChannelTest_PushCloseMulti)
{
    tinycoro::Scheduler scheduler{1};
    tinycoro::BufferedChannel<size_t> channel;

    std::vector<size_t> allValues;
    tinycoro::Mutex mutex;

    auto consumer = [&]() -> tinycoro::Task<void> {
        size_t val;
        if (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
        {
            auto lock = co_await mutex;
            allValues.push_back(val);
        }
    };

    auto producer = [&]()->tinycoro::Task<void> {

        co_await tinycoro::Sleep(50ms);

        channel.Push(39);
        channel.Push(40);
        channel.Push(41);
        channel.PushAndClose(42);

        EXPECT_THROW(channel.Push(33), tinycoro::BufferedChannelException);
    };

    tinycoro::GetAll(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), producer());

    EXPECT_EQ(allValues.size(), 4);

    EXPECT_EQ(allValues[0], 39);
    EXPECT_EQ(allValues[1], 40);
    EXPECT_EQ(allValues[2], 41);
    EXPECT_EQ(allValues[3], 42);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_EmplaceClose)
{
    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel;

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
            channel.Emplace(i);
        }
        else
        {
            channel.EmplaceAndClose(i);
        }
    }

    tinycoro::GetAll(scheduler, consumer());
}

TEST_P(BufferedChannelTest, BufferedChannelTest_EmplaceClose_multi)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel;
    std::mutex                        mtx;
    std::set<size_t>                  allValues;
    size_t                            lastValue{};

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
                channel.Emplace(i);
            }
            else
            {
                channel.EmplaceAndClose(i);
            }
        }

        co_return;
    };

    tinycoro::GetAll(scheduler, consumer(), consumer(), consumer(), consumer(), producer(), consumer(), consumer());

    EXPECT_EQ(allValues.size(), count);
    EXPECT_EQ(lastValue, count - 1);
}

TEST_P(BufferedChannelTest, BufferedChannelTest_PushClose_multi)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto count = GetParam();

    tinycoro::BufferedChannel<size_t> channel;
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
                channel.Emplace(i);
            }
            else
            {
                channel.EmplaceAndClose(i);
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

    tinycoro::GetAll(scheduler, tasks);

    EXPECT_EQ(lastValue, count - 1);
    EXPECT_EQ(allValues.size(), count);
}