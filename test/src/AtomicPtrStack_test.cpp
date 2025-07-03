#include <gtest/gtest.h>

#include <tinycoro/AtomicPtrStack.hpp>
#include <tinycoro/tinycoro_all.h>

struct AtomicStackNode : tinycoro::detail::SingleLinkable<AtomicStackNode>
{
};

class AtomicPtrStackTest : public ::testing::Test {
    protected:
        tinycoro::detail::AtomicPtrStack<AtomicStackNode> stack;
        AtomicStackNode node1, node2, node3;
    };


TEST_F(AtomicPtrStackTest, AtomicPtrStackTest_push)
{
    EXPECT_TRUE(stack.empty());

    stack.try_push(&node1);
    stack.try_push(&node2);
    stack.try_push(&node3);

    EXPECT_FALSE(stack.empty());
}

TEST_F(AtomicPtrStackTest, AtomicPtrStackTest_push_steal)
{
    stack.try_push(&node1);
    stack.try_push(&node2);
    stack.try_push(&node3);

    auto top = stack.steal();

    EXPECT_TRUE(stack.empty());
    EXPECT_FALSE(stack.closed());

    EXPECT_EQ(top, &node3);
    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->next, &node1);
    EXPECT_EQ(top->next->next->next, nullptr);
}

TEST_F(AtomicPtrStackTest, AtomicPtrStackTest_push_close)
{
    stack.try_push(&node1);
    stack.try_push(&node2);
    stack.try_push(&node3);

    auto top = stack.close();

    EXPECT_TRUE(stack.empty());
    EXPECT_TRUE(stack.closed());

    // need to fail
    EXPECT_FALSE(stack.try_push(&node1));

    EXPECT_EQ(top, &node3);
    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->next, &node1);
    EXPECT_EQ(top->next->next->next, nullptr);
}

struct AtomicPtrStackFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AtomicPtrStackFunctionalTest, AtomicPtrStackFunctionalTest, testing::Values(1, 10, 100, 1000, 10000, 100000));

TEST_P(AtomicPtrStackFunctionalTest, AtomicPtrStackFunctionalTest_multi_threaded)
{
    tinycoro::Scheduler scheduler;

    const auto count = GetParam();

    tinycoro::detail::AtomicPtrStack<AtomicStackNode> stack;

    std::vector<AtomicStackNode> vec1{count};
    std::vector<AtomicStackNode> vec2{count};
    std::vector<AtomicStackNode> vec3{count};
    std::vector<AtomicStackNode> vec4{count};

    auto producer = [&stack](auto& vec)->tinycoro::Task<void> {
        for(auto& it : vec)
        {
            EXPECT_TRUE(stack.try_push(&it));
        }
        co_return;
    };
    
    tinycoro::AllOf(scheduler, producer(vec1), producer(vec2), producer(vec3), producer(vec4));

    auto elem = stack.close();

    size_t counter{0};
    while(elem)
    {
        counter++;
        elem = elem->next;
    }

    EXPECT_EQ(counter / 4, count);
}