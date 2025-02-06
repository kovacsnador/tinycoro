#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

template<typename T>
struct Node
{
    Node(T v)
    : val{v}
    {
    }

    Node* next{nullptr};
    Node* prev{nullptr};

    T val;
};

struct LinkedPtrListTest : testing::Test
{
    Node<int32_t> n1{1};
    Node<int32_t> n2{2};
    Node<int32_t> n3{3};

    tinycoro::detail::LinkedPtrList<Node<int32_t>> list;
};

TEST_F(LinkedPtrListTest, LinkedPtrListTest_push)
{
    list.push_front(&n1);
    list.push_front(&n2);
    list.push_front(&n3);

    EXPECT_EQ(n3.next, &n2);
    EXPECT_EQ(n3.prev, nullptr);

    EXPECT_EQ(n2.next, &n1);
    EXPECT_EQ(n2.prev, &n3);

    EXPECT_EQ(n1.next, nullptr);
    EXPECT_EQ(n1.prev, &n2);
}

TEST_F(LinkedPtrListTest, LinkedPtrListTest_erase)
{
    list.push_front(&n1);
    list.push_front(&n2);
    list.push_front(&n3);

    list.erase(&n2);

    EXPECT_EQ(n3.next, &n1);
    EXPECT_EQ(n3.prev, nullptr);

    EXPECT_EQ(n2.next, nullptr);
    EXPECT_EQ(n2.prev, nullptr);

    EXPECT_EQ(n1.next, nullptr);
    EXPECT_EQ(n1.prev, &n3);

    list.erase(&n3);
    
    EXPECT_EQ(n3.next, nullptr);
    EXPECT_EQ(n3.prev, nullptr);

    EXPECT_EQ(n2.next, nullptr);
    EXPECT_EQ(n2.prev, nullptr);

    EXPECT_EQ(n1.next, nullptr);
    EXPECT_EQ(n1.prev, nullptr);

    EXPECT_EQ(&n1, list.begin());

    list.erase(&n1);

    EXPECT_EQ(n3.next, nullptr);
    EXPECT_EQ(n3.prev, nullptr);

    EXPECT_EQ(n2.next, nullptr);
    EXPECT_EQ(n2.prev, nullptr);

    EXPECT_EQ(n1.next, nullptr);
    EXPECT_EQ(n1.prev, nullptr);

    EXPECT_NE(&n1, list.begin());
}

TEST_F(LinkedPtrListTest, LinkedPtrListTest_empty)
{
    EXPECT_TRUE(list.empty());

    list.push_front(&n1);
    list.push_front(&n2);
    list.push_front(&n3);

    EXPECT_FALSE(list.empty());

    list.erase(&n1);
    EXPECT_FALSE(list.empty());

    list.erase(&n2);
    EXPECT_FALSE(list.empty());
    
    list.erase(&n3);
    EXPECT_TRUE(list.empty());
}

TEST_F(LinkedPtrListTest, LinkedPtrListTest_size)
{
    EXPECT_EQ(list.size(), 0);

    list.push_front(&n1);
    EXPECT_EQ(list.size(), 1);

    list.push_front(&n2);
    EXPECT_EQ(list.size(), 2);

    list.push_front(&n3);
    EXPECT_EQ(list.size(), 3);

    list.erase(&n3);
    EXPECT_EQ(list.size(), 2);

    list.erase(&n2);
    EXPECT_EQ(list.size(), 1);
    
    list.erase(&n1);
    EXPECT_EQ(list.size(), 0);
}