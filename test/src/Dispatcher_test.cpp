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

TEST(DispatcherTest, DispatcherTest_small_cache)
{
    tinycoro::detail::AtomicQueue<int32_t, 4> queue;
    tinycoro::detail::Dispatcher              dispatcher{queue, {}};

    EXPECT_TRUE(dispatcher.empty());
    EXPECT_FALSE(dispatcher.full());

    int32_t val;
    EXPECT_FALSE(dispatcher.try_pop(val));

    dispatcher.wait_for_push();
    EXPECT_TRUE(dispatcher.try_push(0));

    dispatcher.wait_for_push();
    EXPECT_TRUE(dispatcher.try_push(1));
    
    dispatcher.wait_for_push();
    EXPECT_TRUE(dispatcher.try_push(2));

    dispatcher.wait_for_push();
    EXPECT_TRUE(dispatcher.try_push(3));

    EXPECT_TRUE(dispatcher.full());

    EXPECT_FALSE(dispatcher.try_push(4));

    dispatcher.wait_for_pop();
    EXPECT_TRUE(dispatcher.try_pop(val));
    EXPECT_EQ(val, 0);

    EXPECT_TRUE(dispatcher.try_push(4));

    EXPECT_FALSE(dispatcher.empty());
    EXPECT_TRUE(dispatcher.full());

    dispatcher.wait_for_pop();
    EXPECT_TRUE(dispatcher.try_pop(val));
    EXPECT_EQ(val, 1);

    dispatcher.wait_for_pop();
    EXPECT_TRUE(dispatcher.try_pop(val));
    EXPECT_EQ(val, 2);

    dispatcher.wait_for_pop();
    EXPECT_TRUE(dispatcher.try_pop(val));
    EXPECT_EQ(val, 3);

    dispatcher.wait_for_pop();
    EXPECT_TRUE(dispatcher.try_pop(val));
    EXPECT_EQ(val, 4);

    dispatcher.wait_for_push();
    EXPECT_TRUE(dispatcher.empty());
}

struct DispatcherTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(DispatcherTest,
                         DispatcherTest,
                         testing::Values(10, 100, 1000, 10000, 100000, 1000000));

TEST_P(DispatcherTest, DispatcherTest_wait_for_pop)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<size_t, 1024> queue;
    tinycoro::detail::Dispatcher                dispatcher{queue, {}};

    auto fut = std::async(std::launch::async, [&] {
        for (size_t i = 0; i < count;)
        {
            // wait until we can pop
            dispatcher.wait_for_pop();

            size_t val;
            if (dispatcher.try_pop(val))
            {
                EXPECT_EQ(val, i++);
            }
        }
    });

    for (size_t i = 0; i < count;)
    {
        auto succeed = dispatcher.try_push(i);

        if (i < queue.capacity())
        {
            EXPECT_TRUE(succeed);
        };

        if (succeed)
            i++;
    }

    // wait for the future
}

TEST_P(DispatcherTest, DispatcherTest_wait_for_push)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<size_t, 2> queue;
    tinycoro::detail::Dispatcher              dispatcher{queue, {}};

    auto asyncFunc = [&] {
        for (size_t i = 0; i < count; i++)
        {
            // wait until we can pop
            dispatcher.wait_for_pop();

            EXPECT_FALSE(dispatcher.empty());

            size_t val;
            EXPECT_TRUE(dispatcher.try_pop(val));

            EXPECT_EQ(val, i);
        }
    };

    auto fut = std::async(std::launch::async, asyncFunc);

    for (size_t i = 0; i < count; i++)
    {
        dispatcher.wait_for_push();

        EXPECT_FALSE(dispatcher.full());

        EXPECT_TRUE(dispatcher.try_push(i));
    }

    // wait for the future
}

// TODO check and fix this!!!!
TEST_P(DispatcherTest, DispatcherTest_wait_for_push_full_queue)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<size_t, 2> queue;
    tinycoro::detail::Dispatcher              dispatcher{queue, {}};

    // this need to move
    dispatcher.wait_for_push();

    // make the queue full
    EXPECT_TRUE(dispatcher.try_push(0));
    EXPECT_TRUE(dispatcher.try_push(1));

    // this need to move
    dispatcher.wait_for_pop();

    auto asyncFunc = [&] {
        for (size_t i = 0; i < count; i++)
        {
            // wait until we can pop
            dispatcher.wait_for_pop();

            EXPECT_FALSE(dispatcher.empty());

            size_t val;
            EXPECT_TRUE(dispatcher.try_pop(val));

            EXPECT_EQ(val, i);
        }
    };


    auto fut = std::async(std::launch::async, asyncFunc);

    for (size_t i = 2; i < count; i++)
    {
        dispatcher.wait_for_push();

        EXPECT_FALSE(dispatcher.full());

        EXPECT_TRUE(dispatcher.try_push(i));
    }

    // wait for the future
}

TEST_P(DispatcherTest, DispatcherTest_wait_for_push_mpmc)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<int32_t, 2> queue;
    tinycoro::detail::Dispatcher              dispatcher{queue, {}};

    {
        auto consumer = [&] {
            for (size_t i = 0; i < count;)
            {
                // wait until we can pop
                dispatcher.wait_for_pop();

                int32_t val;
                auto    succeed = dispatcher.try_pop(val);
                if (succeed)
                {
                    i++;
                }
            }
        };

        auto consumer1 = std::async(std::launch::async, consumer);
        auto consumer2 = std::async(std::launch::async, consumer);
        auto consumer3 = std::async(std::launch::async, consumer);
        auto consumer4 = std::async(std::launch::async, consumer);

        auto producer = [&] {
            for (size_t i = 0; i < count;)
            {
                dispatcher.wait_for_push();

                if (dispatcher.try_push(42))
                {
                    i++;
                }
            }
        };

        auto producer1 = std::async(std::launch::async, producer);
        auto producer2 = std::async(std::launch::async, producer);
        auto producer3 = std::async(std::launch::async, producer);
        auto producer4 = std::async(std::launch::async, producer);
    }

    EXPECT_TRUE(dispatcher.empty());
    EXPECT_TRUE(queue.empty());
}