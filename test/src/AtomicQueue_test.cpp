#include <gtest/gtest.h>

#include <ranges>
#include <limits>

#include <tinycoro/tinycoro_all.h>

TEST(AtomicQueueTest, AtomicQueueTest_push_pop)
{
    tinycoro::detail::AtomicQueue<int32_t, 32> queue;

    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_TRUE(queue.try_push(4));

    int32_t val;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 1);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 2);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 3);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 4);
}

TEST(AtomicQueueTest, AtomicQueueTest_push_pop_full)
{
    tinycoro::detail::AtomicQueue<int32_t, 4> queue;

    EXPECT_TRUE(queue.try_push(1));
    EXPECT_TRUE(queue.try_push(2));
    EXPECT_TRUE(queue.try_push(3));
    EXPECT_TRUE(queue.try_push(4));

    // the queue is full
    EXPECT_FALSE(queue.try_push(5));

    int32_t val;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 1);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 2);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 3);

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 4);

    // the queue is empty
    EXPECT_FALSE(queue.try_pop(val));
}

TEST(AtomicQueueTest, AtomicQueueTest_empty)
{
    tinycoro::detail::AtomicQueue<int32_t, 4> queue;

    EXPECT_TRUE(queue.empty());

    EXPECT_TRUE(queue.try_push(1));
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.try_push(2));
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.try_push(3));
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.try_push(4));
    EXPECT_FALSE(queue.empty());

    // the queue is full
    EXPECT_FALSE(queue.try_push(5));
    EXPECT_FALSE(queue.empty());

    int32_t val;
    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 1);
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 2);
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 3);
    EXPECT_FALSE(queue.empty());

    EXPECT_TRUE(queue.try_pop(val));
    EXPECT_EQ(val, 4);
    EXPECT_TRUE(queue.empty());

    // the queue is empty
    EXPECT_FALSE(queue.try_pop(val));
    EXPECT_TRUE(queue.empty());
}

TEST(AtomicQueueTest, AtomicQueueTest_wait_for_push)
{
    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler{2};

    tinycoro::detail::AtomicQueue<size_t, 2> queue;

    tinycoro::AutoEvent event;

    auto producer = [&]() -> tinycoro::Task<void> {
        EXPECT_TRUE(queue.try_push(1u));
        EXPECT_TRUE(queue.try_push(2u));

        event.Set();

        co_await tinycoro::SleepFor(clock, 100ms);

        size_t val;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 1u);
    };

    auto consumer = [&]() -> tinycoro::Task<void> {
        co_await event;

        // here we wait for a possible push
        queue.wait_for_push();

        EXPECT_TRUE(queue.try_push(3u));

        size_t val;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 2u);

        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 3u);
    };

    tinycoro::GetAll(scheduler, producer(), consumer());
}

TEST(AtomicQueueTest, AtomicQueueTest_wait_for_pop)
{
    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler{2};

    tinycoro::detail::AtomicQueue<size_t, 2> queue;

    tinycoro::AutoEvent event;

    auto producer = [&]() -> tinycoro::Task<void> {
        EXPECT_TRUE(queue.try_push(1u));
        EXPECT_TRUE(queue.try_push(2u));

        size_t val;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 1u);

        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 2u);

        event.Set();

        co_await tinycoro::SleepFor(clock, 100ms);

        EXPECT_TRUE(queue.try_push(3u));
        EXPECT_TRUE(queue.try_push(4u));
    };

    auto consumer = [&]() -> tinycoro::Task<void> {
        co_await event;

        // here we wait for a possible push
        queue.wait_for_pop();

        size_t val;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 3u);

        // here we wait for a possible push
        queue.wait_for_pop();

        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, 4u);
    };

    tinycoro::GetAll(scheduler, producer(), consumer());
}

struct AtomicQueueFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_single_threaded)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<size_t, (1 << 14)> queue;

    for (auto i : std::views::iota(0u, count))
    {
        EXPECT_TRUE(queue.try_push(i));
    }

    for (auto i : std::views::iota(0u, count))
    {
        size_t val;
        EXPECT_TRUE(queue.try_pop(val));
        EXPECT_EQ(val, i);
    }
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_multi_threaded_pop)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::detail::AtomicQueue<size_t, (1 << 14)> queue;

    for (auto i : std::views::iota(0u, count))
    {
        EXPECT_TRUE(queue.try_push(i));
    }

    EXPECT_FALSE(queue.empty());

    auto consumer = [&]() -> tinycoro::Task<size_t> {
        size_t c{};
        size_t val;
        while (queue.try_pop(val))
        {
            c++;
        }
        co_return c;
    };

    auto [v1, v2, v3, v4, v5, v6, v7, v8]
        = tinycoro::GetAll(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer());

    auto sum = *v1 + *v2 + *v3 + *v4 + *v5 + *v6 + *v7 + *v8;

    EXPECT_EQ(count, sum);
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_multi_threaded)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::detail::AtomicQueue<size_t, (1 << 17)> queue; // make sure you have enough cache size

    auto producer = [&]() -> tinycoro::Task<void> {
        for (auto i : std::views::iota(0u, count))
        {
            while (queue.try_push(i) == false)
                ;
        }

        co_return;
    };

    tinycoro::GetAll(scheduler, producer(), producer(), producer(), producer(), producer(), producer(), producer(), producer());

    EXPECT_FALSE(queue.empty());

    auto consumer = [&]() -> tinycoro::Task<size_t> {
        size_t c{};
        size_t val;
        while (queue.try_pop(val))
        {
            c++;
        }
        co_return c;
    };

    auto [v1, v2, v3, v4, v5, v6, v7, v8]
        = tinycoro::GetAll(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer());

    auto sum = *v1 + *v2 + *v3 + *v4 + *v5 + *v6 + *v7 + *v8;

    EXPECT_EQ(count * 8, sum);
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_multi_threaded_together)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::detail::AtomicQueue<size_t, (1 << 17)> queue; // make sure you have enough cache size

    auto producer = [&]() -> tinycoro::Task<void> {
        for (auto i : std::views::iota(0u, count))
        {
            while (queue.try_push(i) == false)
                ;
        }

        // indicated last value...
        while (queue.try_push(std::numeric_limits<size_t>::max()) == false)
            ;

        co_return;
    };

    auto consumer = [&]() -> tinycoro::Task<size_t> {
        size_t c{};
        for (;;)
        {
            size_t val;
            if (queue.try_pop(val))
            {
                if (val == std::numeric_limits<size_t>::max())
                {
                    // last value popped
                    break;
                }
                c++;
            }
        }
        co_return c;
    };

    auto [v9, v10, v11, v12, v13, v14, v15, v16, v1, v2, v3, v4, v5, v6, v7, v8] = tinycoro::GetAll(scheduler,
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    producer(),
                                                                                                    consumer(),
                                                                                                    consumer(),
                                                                                                    consumer(),
                                                                                                    consumer(),
                                                                                                    consumer(),
                                                                                                    consumer(),
                                                                                                    consumer(),
                                                                                                    consumer());

    EXPECT_TRUE(queue.empty());

    auto sum = *v1 + *v2 + *v3 + *v4 + *v5 + *v6 + *v7 + *v8;

    EXPECT_EQ(count * 8, sum);
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_small_cache_test)
{
    const auto count = GetParam();

    tinycoro::Scheduler                      scheduler{8};
    tinycoro::detail::AtomicQueue<size_t, 2> queue;

    std::atomic<size_t> totalCount{};

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i <= count; ++i)
        {
            while (queue.try_push(i) == false)
            {
                queue.wait_for_push();
            }
            totalCount++;
        }
        co_return;
    };

    auto consumer = [&]() -> tinycoro::Task<void> {
        for (;;)
        {
            size_t val;
            if (queue.try_pop(val) == false)
            {
                queue.wait_for_pop();
            }
            else
            {
                totalCount--;
                if(val == count)
                    co_return;
            }
        }

        co_return;
    };

    tinycoro::GetAll(scheduler,
                     producer(),
                     producer(),
                     producer(),
                     producer(),
                     consumer(),
                     consumer(),
                     consumer(),
                     consumer());

    EXPECT_EQ(totalCount, 0);
}