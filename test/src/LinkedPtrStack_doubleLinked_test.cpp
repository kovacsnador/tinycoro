#include <gtest/gtest.h>

#include <tinycoro/LinkedPtrStack.hpp>
#include <tinycoro/LinkedUtils.hpp>

#include "ListCommonUtils.hpp"

// Mock Node class
struct MockNodeDS : tinycoro::detail::DoubleLinkable<MockNodeDS> { };

class DoubleLinkedPtrStackTest : public ::testing::Test {
protected:
    tinycoro::detail::LinkedPtrStack<MockNodeDS> stack;
    MockNodeDS node1, node2, node3;
};

TEST_F(DoubleLinkedPtrStackTest, typeTest) 
{
    using NodeType = decltype(stack)::value_type;
    using NodeTypePtr = tinycoro::detail::LinkedPtrStack<MockNodeDS*>::value_type;

    EXPECT_TRUE((std::same_as<NodeType, NodeTypePtr>));
}

// Test: Initially, the stack should be empty
TEST_F(DoubleLinkedPtrStackTest, StackIsInitiallyEmpty) {
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.top(), nullptr);
}

// Test: After pushing one node, the stack should not be empty
TEST_F(DoubleLinkedPtrStackTest, PushOneNode) {
    stack.push(&node1);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.top(), &node1);
}

// Test: After pushing two nodes, top should be the most recently pushed node
TEST_F(DoubleLinkedPtrStackTest, PushTwoNodes) {
    stack.push(&node1);
    stack.push(&node2);
    EXPECT_EQ(stack.top(), &node2);
}

// Test: Pop should return the last pushed node and remove it from the stack
TEST_F(DoubleLinkedPtrStackTest, PopRemovesTopNode) {
    stack.push(&node1);
    stack.push(&node2);
    MockNodeDS* poppedNode = stack.pop();
    
    EXPECT_EQ(poppedNode, &node2);
    EXPECT_EQ(stack.top(), &node1);
}

// Test: Pop should return nullptr when the stack is empty
TEST_F(DoubleLinkedPtrStackTest, PopFromEmptyStackReturnsNull) {
    EXPECT_EQ(stack.pop(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(DoubleLinkedPtrStackTest, PopAllNodesMakesStackEmpty) {
    EXPECT_EQ(stack.last(), nullptr);
    
    stack.push(&node1);
    stack.push(&node2);
    stack.push(&node3);

    EXPECT_EQ(stack.last(), &node1);

    EXPECT_FALSE(stack.empty());
    
    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(node3.next, nullptr);
    EXPECT_EQ(node3.prev, nullptr);

    EXPECT_EQ(&node2, stack.pop());
    EXPECT_EQ(node2.next, nullptr);
    EXPECT_EQ(node2.prev, nullptr);

    EXPECT_EQ(&node1, stack.pop());
    EXPECT_EQ(node1.next, nullptr);
    EXPECT_EQ(node1.prev, nullptr);

    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.top(), nullptr);
}

// Test: Popping all nodes should make the stack empty again
TEST_F(DoubleLinkedPtrStackTest, Size) {
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

TEST_F(DoubleLinkedPtrStackTest, Last) {
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.last(), nullptr);

    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.last(), &node1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node1);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.last(), &node1);
    
    EXPECT_EQ(&node3, stack.pop());
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node1);

    std::ignore = stack.steal();
    EXPECT_EQ(stack.last(), nullptr);

    stack.push(&node3);
    EXPECT_EQ(stack.last(), &node3);
}

TEST_F(DoubleLinkedPtrStackTest, EraseFirst) {
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.last(), nullptr);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.last(), &node1);
    
    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node2);

    auto top = stack.steal();
    EXPECT_EQ(top, &node3);
    EXPECT_EQ(top->prev, nullptr);

    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->prev, &node3);

    EXPECT_EQ(top->next->next, nullptr);
    EXPECT_EQ(stack.last(), nullptr);
}

TEST_F(DoubleLinkedPtrStackTest, EraseMiddle) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.last(), &node1);
    
    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node1);

    auto top = stack.steal();
    EXPECT_EQ(top, &node3);
    EXPECT_EQ(top->prev, nullptr);

    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->prev, &node3);

    EXPECT_EQ(top->next->next, nullptr);
    EXPECT_EQ(stack.last(), nullptr);
}

TEST_F(DoubleLinkedPtrStackTest, EraseLast) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.last(), &node1);
    
    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node1);

    auto top = stack.steal();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->prev, nullptr);

    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->prev, &node2);

    EXPECT_EQ(top->next->next, nullptr);
}

TEST_F(DoubleLinkedPtrStackTest, EraseAll) {
    EXPECT_EQ(stack.size(), 0);
    
    stack.push(&node1);
    EXPECT_EQ(stack.size(), 1);

    stack.push(&node2);
    EXPECT_EQ(stack.size(), 2);

    stack.push(&node3);
    EXPECT_EQ(stack.size(), 3);
    
    EXPECT_EQ(stack.last(), &node1);

    EXPECT_TRUE(stack.erase(&node3));
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.last(), &node1);

    auto top = stack.top();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->prev, nullptr);
    
    EXPECT_EQ(top->next, &node1);
    EXPECT_EQ(top->next->prev, &node2);

    EXPECT_EQ(top->next->next, nullptr);

    EXPECT_TRUE(stack.erase(&node1));
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.last(), &node2);

    top = stack.top();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->prev, nullptr);
    EXPECT_EQ(top->next, nullptr);

    EXPECT_TRUE(stack.erase(&node2));
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.last(), nullptr);

    EXPECT_TRUE(stack.empty());

    EXPECT_EQ(stack.top(), nullptr);
}

struct DoubleLinkedPtrStackFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(DoubleLinkedPtrStackFunctionalTest, DoubleLinkedPtrStackFunctionalTest, testing::Values(1, 10, 100, 1000));

TEST_P(DoubleLinkedPtrStackFunctionalTest, DoubleLinkedPtrStackFunctionalTest_reverse_erase)
{
    const auto count = GetParam();

    tinycoro::test::ReverseCheck<MockNodeDS, tinycoro::detail::LinkedPtrStack>(count);
}

TEST_P(DoubleLinkedPtrStackFunctionalTest, DoubleLinkedPtrStackFunctionalTest_erase)
{
    const auto count = GetParam();

    tinycoro::test::OrderCheck<MockNodeDS, tinycoro::detail::LinkedPtrStack>(count);

}