#include <gtest/gtest.h>

#include <tinycoro/LinkedPtrStack.hpp>

// Mock Node class
struct MockNode {
    MockNode* next = nullptr;
};

class LinkedPtrStackTest : public ::testing::Test {
protected:
    tinycoro::detail::LinkedPtrStack<MockNode> stack;
    MockNode node1, node2, node3;
};

TEST_F(LinkedPtrStackTest, LinkedPtrStackTest_typeTest) 
{
    using NodeType = decltype(stack)::value_type;
    using NodeTypePtr = tinycoro::detail::LinkedPtrStack<MockNode*>::value_type;

    EXPECT_TRUE((std::same_as<NodeType, NodeTypePtr>));
}

// Test: Initially, the stack should be empty
TEST_F(LinkedPtrStackTest, StackIsInitiallyEmpty) {
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.top(), nullptr);
}

// Test: After pushing one node, the stack should not be empty
TEST_F(LinkedPtrStackTest, PushOneNode) {
    stack.push(&node1);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.top(), &node1);
}

// Test: After pushing two nodes, top should be the most recently pushed node
TEST_F(LinkedPtrStackTest, PushTwoNodes) {
    stack.push(&node1);
    stack.push(&node2);
    EXPECT_EQ(stack.top(), &node2);
}

// Test: Pop should return the last pushed node and remove it from the stack
TEST_F(LinkedPtrStackTest, PopRemovesTopNode) {
    stack.push(&node1);
    stack.push(&node2);
    MockNode* poppedNode = stack.pop();
    
    EXPECT_EQ(poppedNode, &node2);
    EXPECT_EQ(stack.top(), &node1);
}

// Test: Pop should return nullptr when the stack is empty
TEST_F(LinkedPtrStackTest, PopFromEmptyStackReturnsNull) {
    EXPECT_EQ(stack.pop(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(LinkedPtrStackTest, PopAllNodesMakesStackEmpty) {
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    EXPECT_FALSE(stack.empty());
    
    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(&node1, stack.pop());

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.top(), nullptr);
}