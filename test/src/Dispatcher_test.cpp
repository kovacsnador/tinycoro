#include <gtest/gtest.h>

#include <ranges>
#include <future>
#include <chrono>

#include "tinycoro/Dispatcher.hpp"
#include "tinycoro/AtomicQueue.hpp"

using namespace std::chrono_literals;

TEST(DispatcherTest, DispatcherTest_local_notify)
{
    std::atomic<uint32_t> atom{};

    for (auto it : std::views::iota(1u, 100u))
    {
        tinycoro::detail::local::Notify(atom, std::atomic_notify_one);
        EXPECT_EQ(atom.load(), it);
    }
}

TEST(DispatcherTest, DispatcherTest_local_notify2)
{
    std::atomic<uint32_t> atom{};

    auto fut = std::async(std::launch::async, [&] {
        atom.wait(0);
        SUCCEED();
    });

    tinycoro::detail::local::Notify(atom, std::atomic_notify_one);
    EXPECT_EQ(atom.load(), 1);

    fut.get();
}

namespace test {
}

TEST(DispatcherTest, DispatcherTest_try_push_pop)
{
    tinycoro::detail::AtomicQueue<int32_t, 256> queue;
    tinycoro::detail::Dispatcher                dispatcher{queue, {}};

    EXPECT_TRUE(dispatcher.empty());
    EXPECT_FALSE(dispatcher.full());

    for (auto it : std::views::iota(0u, 42u))
    {
        EXPECT_TRUE(dispatcher.try_push(it));
    }

    for (auto it : std::views::iota(0u, 42u))
    {
        int32_t val;
        EXPECT_TRUE(dispatcher.try_pop(val));
        EXPECT_EQ(val, it);
    }

    EXPECT_TRUE(dispatcher.empty());
}

TEST(DispatcherTest, DispatcherTest_wait_for_pop)
{
    tinycoro::detail::AtomicQueue<int32_t, 1024> queue;
    tinycoro::detail::Dispatcher                dispatcher{queue, {}};

    auto fut = std::async(std::launch::async, [&] {

        for (size_t i = 0; i < 1000;)
        {
            // wait until we can pop
            dispatcher.wait_for_pop();

            int32_t val;
            if (dispatcher.try_pop(val))
            {
                EXPECT_EQ(val, i++);
            }
        }
    });

    for (auto it : std::views::iota(0, 1000))
    {
        EXPECT_TRUE(dispatcher.try_push(it));
    }

    // wait for the future
    fut.get();
}

// TODO check and fix this!!!!
TEST(DispatcherTest, DispatcherTest_wait_for_push)
{
    tinycoro::detail::AtomicQueue<int32_t, 2> queue;
    tinycoro::detail::Dispatcher              dispatcher{queue, {}};

    // this need to move
    dispatcher.wait_for_push();

    // make the queue full
    EXPECT_TRUE(dispatcher.try_push(0));
    EXPECT_TRUE(dispatcher.try_push(1));

    // this need to move
    dispatcher.wait_for_pop();

    auto asyncFunc = [&] {
        for (size_t i = 0; i < 1000;)
        {
            // wait until we can pop
            dispatcher.wait_for_pop();

            int32_t val;
            if (dispatcher.try_pop(val))
                EXPECT_EQ(val, i++);
        }

        SUCCEED();
    };


    auto fut = std::async(std::launch::async, asyncFunc);

    for (int32_t i = 2; i < 1000;)
    {
        dispatcher.wait_for_push();

        if (dispatcher.try_push(i))
            i++;
    }

    // wait for the future
    fut.get();
}