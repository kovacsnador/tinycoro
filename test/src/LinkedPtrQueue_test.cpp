#include <gtest/gtest.h>

#include <tinycoro/LinkedPtrQueue.hpp>

// Mock Node class
struct MockNode {
    MockNode* next = nullptr;
};

class LinkedPtrQueueTest : public ::testing::Test {
protected:
    tinycoro::detail::LinkedPtrQueue<MockNode> stack;
    MockNode node1, node2, node3;
};

TEST_F(LinkedPtrQueueTest, LinkedPtrQueueTest_typeTest) 
{
    using NodeType = decltype(stack)::value_type;
    using NodeTypePtr = tinycoro::detail::LinkedPtrQueue<MockNode*>::value_type;

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
    MockNode* poppedNode = stack.pop();
    
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