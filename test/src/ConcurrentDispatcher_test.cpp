#include <gtest/gtest.h>

#include <tinycoro/Scheduler.hpp>
#include <tinycoro/MPSCPtrQueue.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>

using namespace std::chrono_literals;

namespace {
    struct IntNode : tinycoro::detail::SingleLinkable<IntNode>
    {
        explicit IntNode(int v)
        : value(v)
        {
        }

        int value{};
    };

    struct TrackedNode : tinycoro::detail::SingleLinkable<TrackedNode>
    {
        ~TrackedNode()
        {
            ++destroyed;
        }

        static inline std::atomic<int> destroyed{0};
    };

    using IntTask = std::unique_ptr<IntNode>;
    using IntQueue = tinycoro::detail::MPSCPtrQueue<IntNode>;
    using IntDispatcher = tinycoro::detail::ConcurrentDispatcher<IntQueue, IntTask>;

    using TrackedTask = std::unique_ptr<TrackedNode>;
    using TrackedQueue = tinycoro::detail::MPSCPtrQueue<TrackedNode>;
    using TrackedDispatcher = tinycoro::detail::ConcurrentDispatcher<TrackedQueue, TrackedTask>;

    template<typename T>
    struct QueueFailed
    {
        using value_type = std::add_pointer_t<std::remove_pointer_t<T>>;

        [[nodiscard]] bool try_push(value_type) noexcept { return false; }

        [[nodiscard]] bool try_pop(value_type&) noexcept { return false; }
        
        [[nodiscard]] bool empty() const noexcept { return true; }

        [[nodiscard]] constexpr static size_t capacity() noexcept { return std::numeric_limits<size_t>::max(); } 
    };

} // namespace

TEST(ConcurrentDispatcherTest, try_push_pop_transfers_ownership)
{
    IntQueue queue{};
    IntDispatcher dispatcher{queue};

    IntTask in = std::make_unique<IntNode>(42);
    EXPECT_TRUE(dispatcher.try_push(std::move(in)));
    EXPECT_EQ(in, nullptr);

    IntTask out;
    EXPECT_TRUE(dispatcher.try_pop(out));
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out->value, 42);
}

TEST(ConcurrentDispatcherTest, try_push_failed)
{
    QueueFailed<IntNode> queue;
    tinycoro::detail::ConcurrentDispatcher<decltype(queue), IntTask> dispatcher{queue};

    IntTask in = std::make_unique<IntNode>(42);
    EXPECT_FALSE(dispatcher.try_push(std::move(in)));
    EXPECT_NE(in, nullptr);
    EXPECT_EQ(in->value, 42);
}

TEST(ConcurrentDispatcherTest, try_pop_empty_returns_false)
{
    IntQueue queue{};
    IntDispatcher dispatcher{queue};

    IntTask out;
    EXPECT_FALSE(dispatcher.try_pop(out));
    EXPECT_EQ(out, nullptr);
}

TEST(ConcurrentDispatcherTest, destructor_drains_remaining_tasks)
{
    TrackedNode::destroyed.store(0, std::memory_order_relaxed);

    TrackedQueue queue{};
    {
        TrackedDispatcher dispatcher{queue};
        for (int i = 0; i < 5; ++i)
        {
            EXPECT_TRUE(dispatcher.try_push(std::make_unique<TrackedNode>()));
        }
    }

    EXPECT_EQ(TrackedNode::destroyed.load(std::memory_order_relaxed), 5);
}

TEST(ConcurrentDispatcherTest, wait_for_pop_unblocks_after_push)
{
    IntQueue queue{};
    IntDispatcher dispatcher{queue};

    auto fut = std::async(std::launch::async, [&dispatcher] {
        dispatcher.wait_for_pop(dispatcher.pop_state());

        IntTask out;
        if (dispatcher.try_pop(out) == false || out == nullptr)
            return -1;

        return out->value;
    });

    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(dispatcher.try_push(std::make_unique<IntNode>(7)));

    ASSERT_EQ(fut.wait_for(1s), std::future_status::ready);
    EXPECT_EQ(fut.get(), 7);
}

TEST(ConcurrentDispatcherTest, notify_all_unblocks_wait_for_push)
{
    IntQueue queue{};
    IntDispatcher dispatcher{queue};

    auto fut = std::async(std::launch::async, [&dispatcher] {
        dispatcher.wait_for_push(dispatcher.push_state());
        return true;
    });

    std::this_thread::sleep_for(10ms);
    dispatcher.notify_all();

    ASSERT_EQ(fut.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(fut.get());
}

TEST(ConcurrentDispatcherTest, notify_all_unblocks_wait_for_pop)
{
    IntQueue queue{};
    IntDispatcher dispatcher{queue};

    auto fut = std::async(std::launch::async, [&dispatcher] {
        dispatcher.wait_for_pop(dispatcher.pop_state());
        return true;
    });

    std::this_thread::sleep_for(10ms);
    dispatcher.notify_all();

    ASSERT_EQ(fut.wait_for(1s), std::future_status::ready);
    EXPECT_TRUE(fut.get());
}
