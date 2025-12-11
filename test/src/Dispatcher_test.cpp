#include <gtest/gtest.h>

#include <ranges>
#include <future>

#include "tinycoro/Dispatcher.hpp"
#include "tinycoro/AtomicQueue.hpp"

TEST(DispatcherTest, DispatcherTest_local_notify)
{
    std::atomic<uint32_t> atom{};

    for(auto it : std::views::iota(1u, 100u))
    {
        tinycoro::detail::local::Notify(atom);
        EXPECT_EQ(atom.load(), it);
    }
}

TEST(DispatcherTest, DispatcherTest_local_notify2)
{
    std::atomic<uint32_t> atom{};

    auto fut = std::async(std::launch::async, [&]{
        atom.wait(0);
        SUCCEED();
    });

    tinycoro::detail::local::Notify(atom);
    EXPECT_EQ(atom.load(), 1);

    fut.get();
}

namespace test
{ 
}

TEST(DispatcherTest, DispatcherTest_try_push_pop)
{
    tinycoro::detail::AtomicQueue<int32_t, 256> queue;
    tinycoro::detail::Dispatcher dispatcher{queue};

    EXPECT_TRUE(dispatcher.empty());
    EXPECT_FALSE(dispatcher.full());

    for(auto it : std::views::iota(0u, 42u))
    {
        EXPECT_TRUE(dispatcher.try_push(it));
    }

    for(auto it : std::views::iota(0u, 42u))
    {   
        int32_t val;
        EXPECT_TRUE(dispatcher.try_pop(val));
        EXPECT_EQ(val, it);
    }

    EXPECT_TRUE(dispatcher.empty());
}