#include <gtest/gtest.h>

#include <ranges>
#include <limits>
#include <future>

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

struct AtomicQueueFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AtomicQueueFunctionalTest,
                         AtomicQueueFunctionalTest,
                         testing::Values(1, 10, 100, 1000, 10000, 100000));

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_single_threaded)
{
    const auto count = GetParam();

    // capacity ~130.000
    tinycoro::detail::AtomicQueue<size_t, (1 << 17)> queue;

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

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_push_pop_success)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<size_t, (1 << 20)> queue;

    {
        auto producer = [&] {
            for (size_t i = 0; i < count; ++i)
                EXPECT_TRUE(queue.try_push(i));
        };

        auto producer1 = std::async(std::launch::async, producer);
        auto producer2 = std::async(std::launch::async, producer);
        auto producer3 = std::async(std::launch::async, producer);
        auto producer4 = std::async(std::launch::async, producer);
        auto producer5 = std::async(std::launch::async, producer);
        auto producer6 = std::async(std::launch::async, producer);
        auto producer7 = std::async(std::launch::async, producer);
        auto producer8 = std::async(std::launch::async, producer);
    }

    {
        auto consumer = [&] { 
            size_t val{};
            for (size_t i = 0; i < count; ++i)
                EXPECT_TRUE(queue.try_pop(val));
        };

        auto consumer1 = std::async(std::launch::async, consumer);
        auto consumer2 = std::async(std::launch::async, consumer);
        auto consumer3 = std::async(std::launch::async, consumer);
        auto consumer4 = std::async(std::launch::async, consumer);
        auto consumer5 = std::async(std::launch::async, consumer);
        auto consumer6 = std::async(std::launch::async, consumer);
        auto consumer7 = std::async(std::launch::async, consumer);
        auto consumer8 = std::async(std::launch::async, consumer);
    }

    EXPECT_TRUE(queue.empty());
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_multi_threaded_pop)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::detail::AtomicQueue<size_t, (1 << 17)> queue;

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
        = tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer());

    auto sum = *v1 + *v2 + *v3 + *v4 + *v5 + *v6 + *v7 + *v8;

    EXPECT_EQ(count, sum);
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_multi_threaded)
{
        const auto count = GetParam();

        tinycoro::Scheduler scheduler;

        tinycoro::detail::AtomicQueue<size_t, (1 << 20)> queue; // make sure you have enough cache size

        auto producer = [&]() -> tinycoro::Task<void> {
            for (auto i : std::views::iota(0u, count))
            {
                while (queue.try_push(i) == false)
                    ;
            }

            co_return;
        };

        tinycoro::AllOf(scheduler, producer(), producer(), producer(), producer(), producer(), producer(), producer(), producer());

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
            = tinycoro::AllOf(scheduler, consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer(), consumer());

        auto sum = *v1 + *v2 + *v3 + *v4 + *v5 + *v6 + *v7 + *v8;

        EXPECT_EQ(count * 8, sum);
}

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_multi_threaded_together)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    tinycoro::detail::AtomicQueue<size_t, (1 << 20)> queue; // make sure you have enough cache size

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

    auto [v9, v10, v11, v12, v13, v14, v15, v16, v1, v2, v3, v4, v5, v6, v7, v8] = tinycoro::AllOf(scheduler,
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
    tinycoro::detail::Dispatcher             dispatcher{queue, {}};

    std::atomic<size_t> totalCount{};

    auto producer = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i <= count; ++i)
        {
            while (dispatcher.try_push(i) == false)
            {
                dispatcher.wait_for_push();
            }
            totalCount++;
        }
        co_return;
    };

    auto consumer = [&]() -> tinycoro::Task<void> {
        for (;;)
        {
            size_t val;
            if (dispatcher.try_pop(val) == false)
            {
                dispatcher.wait_for_pop();
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

    tinycoro::AllOf(scheduler,
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

TEST_P(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_wait_for_push_mpmc)
{
    const auto count = GetParam();

    tinycoro::detail::AtomicQueue<int32_t, 2> queue;

    {
        auto consumer = [&] {
            for (size_t i = 0; i < count;)
            {
                int32_t val;
                auto    succeed = queue.try_pop(val);
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
                if (queue.try_push(42))
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

    EXPECT_TRUE(queue.empty());
}

// Currently disabled, the overflow is not solved in AtomicQueue.
/* TEST(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_overflow_test_1)
{
    tinycoro::detail::AtomicQueue<size_t, 256, uint8_t> queue;


    for (size_t j = 0; j < 100; ++j)
    {
        for (size_t i = 0; i < 256; ++i)
        {
            auto success = queue.try_push(i);
            EXPECT_TRUE(success);
        }

        for (size_t i = 0; i < 256; ++i)
        {
            size_t val;
            auto success = queue.try_pop(val);
            EXPECT_TRUE(success);
            EXPECT_EQ(i, val);
        }
    }
}

TEST(AtomicQueueFunctionalTest, AtomicQueueFunctionalTest_overflow_test_2)
{
    tinycoro::detail::AtomicQueue<size_t, 256, uint8_t> queue;

    for (size_t i = 0; i < 128; ++i)
    {
        auto success = queue.try_push(i);
        EXPECT_TRUE(success);
    }

    for (size_t i = 0; i < 28; ++i)
    {
        size_t val;
        auto   success = queue.try_pop(val);
        EXPECT_TRUE(success);
        EXPECT_EQ(i, val);
    }
    // we have here 100 element
    // but the head is at 128

    // make it full with overload
    for (size_t i = 128; i < 284; ++i)
    {
        auto success = queue.try_push(i);
        EXPECT_TRUE(success);
    }

    EXPECT_FALSE(queue.try_push(257));

    for (size_t i = 28; i < 284; ++i)
    {
        size_t val;
        auto   success = queue.try_pop(val);
        EXPECT_TRUE(success);
        EXPECT_EQ(i, val);
    }
}*/
