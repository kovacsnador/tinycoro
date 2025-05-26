#include <gtest/gtest.h>

#include <tinycoro/LinkedPtrQueue.hpp>

#include "ListCommonUtils.hpp"

// Mock Node class
struct MockNodeQ {
    MockNodeQ* next = nullptr;
};

class LinkedPtrQueueTest : public ::testing::Test {
protected:
    tinycoro::detail::LinkedPtrQueue<MockNodeQ> stack;
    MockNodeQ node1, node2, node3;
};

TEST_F(LinkedPtrQueueTest, typeTest) 
{
    using NodeType = decltype(stack)::value_type;
    using NodeTypePtr = tinycoro::detail::LinkedPtrQueue<MockNodeQ*>::value_type;

    EXPECT_TRUE((std::same_as<NodeType, NodeTypePtr>));
}

// Test: Initially, the stack should be empty
TEST_F(LinkedPtrQueueTest, StackIsInitiallyEmpty) {
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.begin(), nullptr);
}

// Test: After pushing one node, the stack should not be empty
TEST_F(LinkedPtrQueueTest, PushOneNode) {
    stack.push(&node1);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.begin(), &node1);
}

// Test: After pushing two nodes, begin should be the most recently pushed node
TEST_F(LinkedPtrQueueTest, PushTwoNodes) {
    stack.push(&node1);
    stack.push(&node2);
    EXPECT_EQ(stack.begin(), &node1);
}

// Test: Pop should return the last pushed node and remove it from the stack
TEST_F(LinkedPtrQueueTest, PopRemovesTopNode) {
    stack.push(&node1);
    stack.push(&node2);
    MockNodeQ* poppedNode = stack.pop();
    
    EXPECT_EQ(poppedNode->next, nullptr);

    EXPECT_EQ(poppedNode, &node1);
    EXPECT_EQ(stack.begin(), &node2);
}

// Test: Pop should return nullptr when the stack is empty
TEST_F(LinkedPtrQueueTest, PopFromEmptyStackReturnsNull) {
    EXPECT_EQ(stack.pop(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(LinkedPtrQueueTest, PopAllNodesMakesStackEmpty) {
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    EXPECT_FALSE(stack.empty());
    
    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(&node3, stack.pop());

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.begin(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(LinkedPtrQueueTest, Size) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(stack.size(), 2);

    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(stack.size(), 1);

    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(stack.size(), 0);

    // call pop on empty stack
    EXPECT_EQ(nullptr, stack.pop());
    EXPECT_EQ(stack.size(), 0);
}

TEST_F(LinkedPtrQueueTest, EraseFirst) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(node1.next, nullptr);
    EXPECT_EQ(stack.size(), 2);

    auto top = stack.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(LinkedPtrQueueTest, EraseMiddle) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(stack.size(), 2);

    auto top = stack.steal();
    EXPECT_EQ(top, &node1);
    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(LinkedPtrQueueTest, EraseLast) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node3.next, nullptr);
    EXPECT_EQ(stack.size(), 2);

    EXPECT_EQ(stack.pop(), &node1);

    auto top = stack.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, nullptr);

    stack.push(&node1);

    top = stack.steal();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(LinkedPtrQueueTest, Concat) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);
    
    MockNodeQ node4, node5, node6;
    tinycoro::detail::LinkedPtrQueue<MockNodeQ> stack2;

    stack2.push(&node4);
    stack2.push(&node5);
    stack2.push(&node6);

    stack.concat(stack2);
    EXPECT_EQ(stack.size(), 6);

    EXPECT_EQ(node3.next, &node4);

    size_t count{};
    auto it = stack.steal();
    while (it != nullptr)
    {
        count++;
        it = it->next;
    }
    EXPECT_EQ(count, 6);    
}

TEST_F(LinkedPtrQueueTest, push_front) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);
    
    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(node2.next, nullptr);

    stack.push_front(&node2);
    EXPECT_EQ(node2.next, &node1);

    EXPECT_EQ(stack.pop(), &node2);

    stack.push_front(&node2);
    EXPECT_EQ(node2.next, &node1);

    EXPECT_EQ(stack.pop(), &node2);
}

TEST_F(LinkedPtrQueueTest, push_front_first_elem) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);
    
    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(node1.next, nullptr);

    stack.push_front(&node1);
    EXPECT_EQ(node1.next, &node2);

    EXPECT_EQ(stack.pop(), &node1);
}

TEST_F(LinkedPtrQueueTest, Concat_empty) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);
    
    tinycoro::detail::LinkedPtrQueue<MockNodeQ> stack2;

    stack.concat(stack2);
    EXPECT_EQ(stack.size(), 3);

    size_t count{};
    auto it = stack.steal();
    while (it != nullptr)
    {
        count++;
        it = it->next;
    }
    EXPECT_EQ(count, 3); 
}

TEST_F(LinkedPtrQueueTest, Concat_empty_2) {
    EXPECT_EQ(stack.size(), 0);
    
    tinycoro::detail::LinkedPtrQueue<MockNodeQ> stack2;

    stack2.push(&node1);
    stack2.push(&node2);
    stack2.push(&node3);

    stack.concat(stack2);
    EXPECT_EQ(stack.size(), 3);

    auto it = stack.steal();
    EXPECT_EQ(it, &node1);
    EXPECT_EQ(it->next, &node2);
    EXPECT_EQ(it->next->next, &node3);
    EXPECT_EQ(it->next->next->next, nullptr);
}

TEST_F(LinkedPtrQueueTest, EraseAll) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(stack.size(), 2);

    auto top = stack.begin();
    EXPECT_EQ(top, &node1);
    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->next, nullptr);

    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(stack.size(), 1);

    top = stack.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, nullptr);

    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(stack.size(), 0);

    EXPECT_TRUE(stack.empty());

    EXPECT_EQ(stack.begin(), nullptr);
}

struct LinkedPtrQueueFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(LinkedPtrQueueFunctionalTest, LinkedPtrQueueFunctionalTest, testing::Values(1, 10, 100, 1000));

TEST_P(LinkedPtrQueueFunctionalTest, LinkedPtrQueueFunctionalTest_reverse_erase)
{
    const auto count = GetParam();

    tinycoro::test::ReverseCheck<MockNodeQ, tinycoro::detail::LinkedPtrQueue>(count);
}

TEST_P(LinkedPtrQueueFunctionalTest, LinkedPtrQueueFunctionalTest_erase)
{
    const auto count = GetParam();

    tinycoro::test::OrderCheck<MockNodeQ, tinycoro::detail::LinkedPtrQueue>(count);
}