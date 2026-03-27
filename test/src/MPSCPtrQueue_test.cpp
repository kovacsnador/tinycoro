#include <gtest/gtest.h>

#include <array>
#include <thread>
#include <vector>

#include <tinycoro/MPSCPtrQueue.hpp>
#include <tinycoro/LinkedUtils.hpp>

struct MPSCNode : tinycoro::detail::SingleLinkable<MPSCNode>
{
    size_t id{};
};

TEST(MPSCPtrQueueTest, push_pop_fifo_single_thread)
{
    tinycoro::detail::MPSCPtrQueue<MPSCNode> queue;
    MPSCNode n1{.id = 1};
    MPSCNode n2{.id = 2};
    MPSCNode n3{.id = 3};

    EXPECT_TRUE(queue.empty());

    EXPECT_TRUE(queue.try_push(&n1));
    EXPECT_TRUE(queue.try_push(&n2));
    EXPECT_TRUE(queue.try_push(&n3));
    EXPECT_FALSE(queue.empty());

    MPSCNode* ptr{};

    EXPECT_TRUE(queue.try_pop(ptr));
    EXPECT_EQ(ptr->id, 1u);

    EXPECT_TRUE(queue.try_pop(ptr));
    EXPECT_EQ(ptr->id, 2u);

    EXPECT_TRUE(queue.try_pop(ptr));
    EXPECT_EQ(ptr->id, 3u);

    EXPECT_FALSE(queue.try_pop(ptr));
    EXPECT_TRUE(queue.empty());
}

TEST(MPSCPtrQueueTest, multi_producer_all_elements_observed)
{
    constexpr size_t producersCount = 8;
    constexpr size_t elementsPerProducer = 100'000;
    constexpr size_t totalElements = producersCount * elementsPerProducer;

    tinycoro::detail::MPSCPtrQueue<MPSCNode> queue;
    std::array<std::vector<MPSCNode>, producersCount> producersData;
    std::array<std::thread, producersCount> producers;

    for (size_t producerIdx = 0; producerIdx < producersCount; ++producerIdx)
    {
        auto& data = producersData[producerIdx];
        data.resize(elementsPerProducer);
        for (size_t i = 0; i < elementsPerProducer; ++i)
        {
            data[i].id = producerIdx * elementsPerProducer + i;
        }

        producers[producerIdx] = std::thread{[&queue, &data] {
            for (auto& node : data)
            {
                EXPECT_TRUE(queue.try_push(&node));
            }
        }};
    }

    for (auto& producer : producers)
    {
        producer.join();
    }

    std::vector<bool> observed(totalElements, false);
    size_t popCount{};
    MPSCNode* node{};
    while (queue.try_pop(node))
    {
        ASSERT_LT(node->id, totalElements);
        ASSERT_FALSE(observed[node->id]);
        observed[node->id] = true;
        ++popCount;
    }

    EXPECT_EQ(popCount, totalElements);
    EXPECT_TRUE(queue.empty());
}
