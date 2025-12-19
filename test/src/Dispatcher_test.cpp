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
    tinycoro::detail::Dispatcher                dispatcher{queue};

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

/*TEST(DispatcherTest, DispatcherTest_wait_for_pop)
{
    tinycoro::detail::AtomicQueue<int32_t, 256> queue;
    tinycoro::detail::Dispatcher                dispatcher{queue};

    auto fut = std::async(std::launch::async, [&] {
        for (auto it : std::views::iota(0, 100))
        {
            auto state = dispatcher.push_state();

            // wait until we can pop
            dispatcher.wait_for_pop(state);

            int32_t val;
            EXPECT_TRUE(dispatcher.try_pop(val));
            EXPECT_EQ(val, it);
        }

        SUCCEED();
    });

    for (auto it : std::views::iota(0, 100))
    {
        std::this_thread::sleep_for(10ms);
        EXPECT_TRUE(dispatcher.try_push(it));
    }

    // wait for the future
    fut.get();
}*/

// TODO check and fix this!!!!
TEST(DispatcherTest, DispatcherTest_wait_for_push)
{
    tinycoro::detail::AtomicQueue<int32_t, 2> queue;
    tinycoro::detail::Dispatcher              dispatcher{queue};

    auto pushState = dispatcher.push_state(std::memory_order::acquire);
    auto popState = dispatcher.pop_state(std::memory_order::acquire);

    // this need to move
    //dispatcher.wait_for_push(0);

    // make the queue full
    EXPECT_TRUE(dispatcher.try_push(0));
    EXPECT_TRUE(dispatcher.try_push(1));

    // this need to move
    //dispatcher.wait_for_pop(0);

    auto asyncFunc = [&] {
        for (size_t i = 0; i < 100;)
        {
            // wait until we can pop
            dispatcher.wait_for_pop(pushState++);

            //pushState = dispatcher.push_state(std::memory_order::acquire);

            EXPECT_FALSE(dispatcher.empty());

            int32_t val;
            auto succeed = dispatcher.try_pop(val);
            if(succeed)
                i++;

            EXPECT_TRUE(succeed);
        }

        SUCCEED();
    };


    auto fut = std::async(std::launch::async, asyncFunc);

    for (size_t i = 2; i < 100;)
    {
        dispatcher.wait_for_push(popState++);

        //popState = dispatcher.pop_state(std::memory_order::acquire);

        EXPECT_FALSE(dispatcher.full());

        auto succeed = dispatcher.try_push(i);
        if(succeed)
            i++;

        EXPECT_TRUE(succeed);
    }

    // wait for the future
    fut.get();
}