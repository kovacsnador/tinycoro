#include <gtest/gtest.h>

#include <tinycoro/LinkedPtrQueue.hpp>
#include <tinycoro/LinkedUtils.hpp>

#include "ListCommonUtils.hpp"

// Mock Node class
struct MockNodeDQ : tinycoro::detail::DoubleLinkable<MockNodeDQ> { };

class DoubleLinkedPtrQueueTest : public ::testing::Test {
protected:
    tinycoro::detail::LinkedPtrQueue<MockNodeDQ> stack;
    MockNodeDQ node1, node2, node3;
};

TEST_F(DoubleLinkedPtrQueueTest, typeTest) 
{
    using NodeType = decltype(stack)::value_type;
    using NodeTypePtr = tinycoro::detail::LinkedPtrQueue<MockNodeDQ*>::value_type;

    EXPECT_TRUE((std::same_as<NodeType, NodeTypePtr>));
}

// Test: Initially, the stack should be empty
TEST_F(DoubleLinkedPtrQueueTest, StackIsInitiallyEmpty) {
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.begin(), nullptr);
}

// Test: After pushing one node, the stack should not be empty
TEST_F(DoubleLinkedPtrQueueTest, PushOneNode) {
    stack.push(&node1);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.begin(), &node1);
}

// Test: After pushing two nodes, begin should be the most recently pushed node
TEST_F(DoubleLinkedPtrQueueTest, PushTwoNodes) {
    stack.push(&node1);
    stack.push(&node2);
    EXPECT_EQ(stack.begin(), &node1);
}

// Test: Pop should return the last pushed node and remove it from the stack
TEST_F(DoubleLinkedPtrQueueTest, PopRemovesTopNode) {
    stack.push(&node1);
    stack.push(&node2);
    MockNodeDQ* poppedNode = stack.pop();

    EXPECT_EQ(poppedNode->next, nullptr);
    EXPECT_EQ(poppedNode->prev, nullptr);
    
    EXPECT_EQ(poppedNode, &node1);
    EXPECT_EQ(stack.begin(), &node2);
}

// Test: Pop should return nullptr when the stack is empty
TEST_F(DoubleLinkedPtrQueueTest, PopFromEmptyStackReturnsNull) {
    EXPECT_EQ(stack.pop(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(DoubleLinkedPtrQueueTest, PopAllNodesMakesStackEmpty) {
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    EXPECT_FALSE(stack.empty());
    
    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(node1.next, nullptr);
    EXPECT_EQ(node1.prev, nullptr);

    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node2.prev, nullptr);

    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(node3.next, nullptr);
    EXPECT_EQ(node3.prev, nullptr);

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.begin(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(DoubleLinkedPtrQueueTest, Size) {
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

TEST_F(DoubleLinkedPtrQueueTest, Last) {
    EXPECT_EQ(stack.last(), nullptr);
    
    stack.push(&node1);
    EXPECT_EQ(stack.last(), &node1);

    stack.push(&node2);
    EXPECT_EQ(stack.last(), &node2);

    stack.push(&node3);
    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(stack.last(), &node3);

    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(stack.last(), &node3);

    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(stack.last(), nullptr);

    // call pop on empty stack
    EXPECT_EQ(nullptr, stack.pop());
    EXPECT_EQ(stack.size(), 0);
}

TEST_F(DoubleLinkedPtrQueueTest, EraseFirst) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(node1.next, nullptr);
    EXPECT_EQ(node1.prev, nullptr);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node3);

    auto top = stack.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->prev, nullptr);

    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->prev, &node2);

    EXPECT_EQ(top->next->next, nullptr);
    EXPECT_EQ(stack.last(), &node3);
}

TEST_F(DoubleLinkedPtrQueueTest, EraseMiddle) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node2.prev, nullptr);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node3);

    auto top = stack.steal();
    EXPECT_EQ(top, &node1);
    EXPECT_EQ(top->prev, nullptr);

    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->prev, &node1);

    EXPECT_EQ(top->next->next, nullptr);
    EXPECT_EQ(stack.last(), nullptr);
}

TEST_F(DoubleLinkedPtrQueueTest, EraseLast) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node3.next, nullptr);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node2);

    EXPECT_EQ(stack.pop(), &node1);

    auto top = stack.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, nullptr);

    stack.push(&node1);

    top = stack.steal();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->next, nullptr);
    EXPECT_EQ(stack.last(), nullptr);
}

TEST_F(DoubleLinkedPtrQueueTest, push_front) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node2.prev, nullptr);
    EXPECT_EQ(stack.last(), &node3);

    stack.push_front(&node2);
    EXPECT_EQ(node2.next, &node1);
    EXPECT_EQ(node2.prev, nullptr);
    EXPECT_EQ(stack.last(), &node3);

    EXPECT_EQ(stack.pop(), &node2);
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node2.prev, nullptr);

    stack.push_front(&node2);
    EXPECT_EQ(node2.next, &node1);
    EXPECT_EQ(node2.prev, nullptr);

    EXPECT_EQ(stack.pop(), &node2);
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node2.prev, nullptr);
}

TEST_F(DoubleLinkedPtrQueueTest, push_front_first_elem) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(node1.next, nullptr);
    EXPECT_EQ(node1.prev, nullptr);

    EXPECT_EQ(stack.last(), &node3);

    stack.push_front(&node1);
    EXPECT_EQ(node1.next, &node2);
    EXPECT_EQ(node1.prev, nullptr);

    EXPECT_EQ(stack.last(), &node3);
    EXPECT_EQ(stack.pop(), &node1);
}

TEST_F(DoubleLinkedPtrQueueTest, erase_from_empty_queue) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    std::ignore = stack.steal();
    
    EXPECT_FALSE(stack.erase(&node1));
    EXPECT_FALSE(stack.erase(&node2));
    EXPECT_FALSE(stack.erase(&node3));
}

TEST_F(DoubleLinkedPtrQueueTest, EraseAll) {
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.last(), nullptr);

    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.last(), &node1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.last(), &node3);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node2);

    auto top = stack.begin();
    EXPECT_EQ(top, &node1);
    EXPECT_EQ(top->prev, nullptr);

    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->prev, &node1);
    EXPECT_EQ(top->next->next, nullptr);

    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.last(), &node2);

    top = stack.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->prev, nullptr);
    EXPECT_EQ(top->next, nullptr);

    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.last(), nullptr);

    EXPECT_TRUE(stack.empty());

    EXPECT_EQ(stack.begin(), nullptr);
}

struct DoubleLinkedPtrQueueFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(DoubleLinkedPtrQueueFunctionalTest, DoubleLinkedPtrQueueFunctionalTest, testing::Values(1, 10, 100, 1000));

TEST_P(DoubleLinkedPtrQueueFunctionalTest, DoubleLinkedPtrQueueFunctionalTest_reverse_erase)
{
    const auto count = GetParam();

    tinycoro::test::ReverseCheck<MockNodeDQ, tinycoro::detail::LinkedPtrQueue>(count);
}

TEST_P(DoubleLinkedPtrQueueFunctionalTest, DoubleLinkedPtrQueueFunctionalTest_erase)
{
    const auto count = GetParam();

    tinycoro::test::OrderCheck<MockNodeDQ, tinycoro::detail::LinkedPtrQueue>(count);
}