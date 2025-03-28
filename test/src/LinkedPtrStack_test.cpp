#include <gtest/gtest.h>

#include <tinycoro/LinkedPtrStack.hpp>

#include "ListCommonUtils.hpp"


// Mock Node class
struct MockNode {
    MockNode* next = nullptr;
};

class LinkedPtrStackTest : public ::testing::Test {
protected:
    tinycoro::detail::LinkedPtrStack<MockNode> stack;
    MockNode node1, node2, node3;
};

TEST_F(LinkedPtrStackTest, typeTest) 
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
    
    EXPECT_EQ(poppedNode->next, nullptr);
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
    EXPECT_EQ(node3.next, nullptr);

    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(node2.next, nullptr);
    
    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(node1.next, nullptr);

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.top(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(LinkedPtrStackTest, Size) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(stack.size(), 2);

    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(stack.size(), 1);

    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(stack.size(), 0);

    // call pop on empty stack
    EXPECT_EQ(nullptr, stack.pop());
    EXPECT_EQ(stack.size(), 0);
}

TEST_F(LinkedPtrStackTest, EraseFirst) {
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

    auto top = stack.steal();
    EXPECT_EQ(top, &node3);
    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(LinkedPtrStackTest, EraseMiddle) {
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
    EXPECT_EQ(top, &node3);
    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(LinkedPtrStackTest, EraseLast) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(node3.next, nullptr);
    EXPECT_EQ(stack.size(), 2);

    auto top = stack.steal();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(LinkedPtrStackTest, EraseAll) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(stack.size(), 2);

    auto top = stack.top();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->next, nullptr);

    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(stack.size(), 1);

    top = stack.top();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, nullptr);

    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(stack.size(), 0);

    EXPECT_TRUE(stack.empty());

    EXPECT_EQ(stack.top(), nullptr);
}

struct LinkedPtrStackFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(LinkedPtrStackFunctionalTest, LinkedPtrStackFunctionalTest, testing::Values(1, 10, 100, 1000));

TEST_P(LinkedPtrStackFunctionalTest, LinkedPtrStackFunctionalTest_reverse_erase)
{
    const auto count = GetParam();

    tinycoro::test::ReverseCheck<MockNode, tinycoro::detail::LinkedPtrStack>(count);
}

TEST_P(LinkedPtrStackFunctionalTest, LinkedPtrStackFunctionalTest_erase)
{
    const auto count = GetParam();

    tinycoro::test::OrderCheck<MockNode, tinycoro::detail::LinkedPtrStack>(count);
}