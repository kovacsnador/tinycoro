#include <gtest/gtest.h>

#include <functional>
#include <vector>
#include <random>

#include <tinycoro/LinkedPtrOrderedList.hpp>

template<typename T>
struct ListNodeMock
{
    ListNodeMock(T v) 
    : val{v}
    {
    }

    T val;
    ListNodeMock* next{};
    
    auto value() const noexcept { return val; }
};

TEST(LinkedPtrOrderedListTest, empty)
{
    using node_t = ListNodeMock<int32_t>;
    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    EXPECT_EQ(list.size(), 0);
    EXPECT_TRUE(list.empty());

    node_t n1{1};
    list.insert(&n1);

    EXPECT_EQ(list.size(), 1);
    EXPECT_FALSE(list.empty());

    auto range = list.lower_bound(2);
    EXPECT_EQ(range->value(), 1);
    EXPECT_EQ(range->next, nullptr);

    EXPECT_EQ(list.size(), 0);
    EXPECT_TRUE(list.empty());

    n1.val = 3;
    list.insert(&n1);
    EXPECT_EQ(list.size(), 1);
    EXPECT_FALSE(list.empty());

    range = nullptr;
    range = list.lower_bound(200);
    EXPECT_EQ(range->value(), 3);
    EXPECT_EQ(range->next, nullptr);
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_ordered_insert)
{
    using node_t = ListNodeMock<int32_t>;
    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    node_t n1{1};
    node_t n2{2};
    node_t n3{3};
    node_t n4{4};
    node_t n5{5};
    node_t n6{6};

    EXPECT_EQ(list.size(), 0);

    list.insert(&n1);
    EXPECT_EQ(list.size(), 1);

    list.insert(&n2);
    EXPECT_EQ(list.size(), 2);

    list.insert(&n3);
    EXPECT_EQ(list.size(), 3);

    list.insert(&n4);
    EXPECT_EQ(list.size(), 4);

    list.insert(&n5);
    EXPECT_EQ(list.size(), 5);

    list.insert(&n6);
    EXPECT_EQ(list.size(), 6);

    for(int32_t i = 1; i < 7; ++i)
    {
        EXPECT_FALSE(list.empty());

        auto range = list.lower_bound(i);
        EXPECT_EQ(range->value(), i);
        EXPECT_EQ(range->next, nullptr);

        EXPECT_EQ(list.size(), 6 - i);
    }

    EXPECT_EQ(list.size(), 0);
    EXPECT_TRUE(list.empty());
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_reverse_insert)
{
    using node_t = ListNodeMock<int32_t>;
    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    node_t n1{1};
    node_t n2{2};
    node_t n3{3};
    node_t n4{4};
    node_t n5{5};
    node_t n6{6};

    EXPECT_EQ(list.size(), 0);

    list.insert(&n6);
    EXPECT_EQ(list.size(), 1);

    list.insert(&n5);
    EXPECT_EQ(list.size(), 2);

    list.insert(&n4);
    EXPECT_EQ(list.size(), 3);

    list.insert(&n3);
    EXPECT_EQ(list.size(), 4);

    list.insert(&n2);
    EXPECT_EQ(list.size(), 5);

    list.insert(&n1);
    EXPECT_EQ(list.size(), 6);

    for(int32_t i = 1; i < 7; ++i)
    {
        EXPECT_FALSE(list.empty());

        auto range = list.lower_bound(i);
        EXPECT_EQ(range->value(), i);
        EXPECT_EQ(range->next, nullptr);

        EXPECT_EQ(list.size(), 6 - i);
    }

    EXPECT_EQ(list.size(), 0);
    EXPECT_TRUE(list.empty());
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_lower_bound)
{
    using node_t = ListNodeMock<int32_t>;
    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    node_t n1{1};
    node_t n2{2};
    node_t n3{3};
    node_t n4{4};
    node_t n5{5};
    node_t n6{6};

    EXPECT_EQ(list.size(), 0);

    list.insert(&n6);
    EXPECT_EQ(list.size(), 1);

    list.insert(&n5);
    EXPECT_EQ(list.size(), 2);

    list.insert(&n4);
    EXPECT_EQ(list.size(), 3);

    list.insert(&n3);
    EXPECT_EQ(list.size(), 4);

    list.insert(&n2);
    EXPECT_EQ(list.size(), 5);

    list.insert(&n1);
    EXPECT_EQ(list.size(), 6);

    auto range = list.lower_bound(2);

    EXPECT_EQ(list.size(), 4);
    EXPECT_FALSE(list.empty());

    for(int32_t i = 1; range; ++i)
    {
        EXPECT_EQ(range->value(), i);
        range = range->next;
    }

    range = list.lower_bound(6);

    EXPECT_EQ(list.size(), 0);
    EXPECT_TRUE(list.empty());

    for(int32_t i = 3; range; ++i)
    {
        EXPECT_EQ(range->value(), i);
        range = range->next;
    }
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_EraseFirst) {

    tinycoro::detail::LinkedPtrOrderedList<ListNodeMock<int32_t>> list;

    ListNodeMock<int32_t> node1{1};
    ListNodeMock<int32_t> node2{2};
    ListNodeMock<int32_t> node3{3};

    EXPECT_EQ(list.size(), 0);
    
    list.insert(&node1);
    EXPECT_EQ(list.size(), 1);

    list.insert(&node2);
    EXPECT_EQ(list.size(), 2);

    list.insert(&node3);
    EXPECT_EQ(list.size(), 3);
    
    EXPECT_TRUE(list.erase(&node1));
    EXPECT_EQ(list.size(), 2);

    auto top = list.steal();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_EraseMiddle) {

    tinycoro::detail::LinkedPtrOrderedList<ListNodeMock<int32_t>> list;

    ListNodeMock<int32_t> node1{1};
    ListNodeMock<int32_t> node2{2};
    ListNodeMock<int32_t> node3{3};

    EXPECT_EQ(list.size(), 0);
    
    list.insert(&node1);
    EXPECT_EQ(list.size(), 1);

    list.insert(&node2);
    EXPECT_EQ(list.size(), 2);

    list.insert(&node3);
    EXPECT_EQ(list.size(), 3);
    
    EXPECT_TRUE(list.erase(&node2));
    EXPECT_EQ(list.size(), 2);

    auto top = list.steal();
    EXPECT_EQ(top, &node1);
    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_EraseLast) {

    tinycoro::detail::LinkedPtrOrderedList<ListNodeMock<int32_t>> list;

    ListNodeMock<int32_t> node1{1};
    ListNodeMock<int32_t> node2{2};
    ListNodeMock<int32_t> node3{3};

    EXPECT_EQ(list.size(), 0);
    
    list.insert(&node1);
    EXPECT_EQ(list.size(), 1);

    list.insert(&node2);
    EXPECT_EQ(list.size(), 2);

    list.insert(&node3);
    EXPECT_EQ(list.size(), 3);
    
    EXPECT_TRUE(list.erase(&node3));
    EXPECT_EQ(list.size(), 2);

    auto top = list.steal();
    EXPECT_EQ(top, &node1);
    EXPECT_EQ(top->next, &node2);
    EXPECT_EQ(top->next->next, nullptr);
}

TEST(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_EraseAll) {

    tinycoro::detail::LinkedPtrOrderedList<ListNodeMock<int32_t>> list;

    ListNodeMock<int32_t> node1{1};
    ListNodeMock<int32_t> node2{2};
    ListNodeMock<int32_t> node3{3};

    EXPECT_EQ(list.size(), 0);
    
    list.insert(&node1);
    EXPECT_EQ(list.size(), 1);

    list.insert(&node2);
    EXPECT_EQ(list.size(), 2);

    list.insert(&node3);
    EXPECT_EQ(list.size(), 3);
    
    EXPECT_TRUE(list.erase(&node1));
    EXPECT_EQ(list.size(), 2);

    auto top = list.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, &node3);
    EXPECT_EQ(top->next->next, nullptr);

    EXPECT_TRUE(list.erase(&node3));
    EXPECT_EQ(list.size(), 1);

    top = list.begin();
    EXPECT_EQ(top, &node2);
    EXPECT_EQ(top->next, nullptr);

    EXPECT_TRUE(list.erase(&node2));
    EXPECT_EQ(list.size(), 0);

    EXPECT_EQ(list.begin(), nullptr);
}

struct LinkedPtrOrderedListTest : testing::TestWithParam<int32_t>
{
};

INSTANTIATE_TEST_SUITE_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest, testing::Values(1, 10, 100, 1000, 10000));

auto ValidateRange(auto range)
{
    size_t contraCount{};

    while(range != nullptr)
    {
        contraCount++;
        if(range->next)
        {
            EXPECT_FALSE(range->next->val < range->val);
        }
        range = range->next;
    }
    return contraCount;
}

TEST_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_checkOrder)
{
    const auto count = GetParam();

    using node_t = ListNodeMock<int32_t>;
    std::vector<node_t> vec;

    for(int32_t i = 0; i < count; ++i)
    {
        vec.emplace_back(i);
    }

    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    for(auto& it : vec)
    {
        list.insert(&it);
    }

    EXPECT_EQ(list.size(), vec.size());

    auto range = list.steal();

    EXPECT_TRUE(list.empty());

    EXPECT_EQ(ValidateRange(range), count);
}

TEST_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_checkOrder_reverse)
{
    const auto count = GetParam();

    using node_t = ListNodeMock<int32_t>;
    std::vector<node_t> vec;

    for(int32_t i = count; i > 0; --i)
    {
        vec.emplace_back(i);
    }

    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    for(auto& it : vec)
    {
        list.insert(&it);
    }

    EXPECT_EQ(list.size(), vec.size());

    auto range = list.lower_bound(count);

    EXPECT_TRUE(list.empty());

    EXPECT_EQ(ValidateRange(range), count);
}

TEST_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_checkOrder_random_big)
{
    const auto count = GetParam();

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> dist(0, static_cast<double>(std::numeric_limits<int32_t>::max()));

    using node_t = ListNodeMock<int32_t>;
    std::vector<node_t> vec;

    for(int32_t i = count; i > 0; --i)
    {
        vec.emplace_back(static_cast<int32_t>(dist(mt)));
    }

    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    for(auto& it : vec)
    {
        list.insert(&it);
    }

    EXPECT_EQ(list.size(), vec.size());

    auto range = list.steal();

    EXPECT_TRUE(list.empty());

    EXPECT_EQ(ValidateRange(range), count);
}

TEST_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_checkOrder_random_small)
{
    const auto count = GetParam();

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> dist(0, static_cast<double>(50));

    using node_t = ListNodeMock<int32_t>;
    std::vector<node_t> vec;

    for(int32_t i = count; i > 0; --i)
    {
        vec.emplace_back(static_cast<int32_t>(dist(mt)));
    }

    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    for(auto& it : vec)
    {
        list.insert(&it);
    }

    EXPECT_EQ(list.size(), vec.size());

    auto range = list.steal();

    EXPECT_TRUE(list.empty());

    EXPECT_EQ(ValidateRange(range), count);
}

TEST_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_lower_bound_unique)
{
    const auto count = GetParam();

    using node_t = ListNodeMock<int32_t>;
    std::vector<node_t> vec;

    for(int32_t i = 0; i < count; ++i)
    {
        vec.emplace_back(i);
    }

    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    for(auto& it : vec)
    {
        list.insert(&it);
    }

    EXPECT_EQ(list.size(), vec.size());

    const int32_t step{10};
    int32_t bound{step};
    while(list.empty() == false)
    {
        auto sizeBefore = list.size();
        auto range = list.lower_bound(bound);
        auto elemCount = ValidateRange(range);

        // this only works because of unique elements
        EXPECT_EQ(sizeBefore - elemCount, list.size());
        bound += step;
    }

    EXPECT_TRUE(list.empty());
}

TEST_P(LinkedPtrOrderedListTest, LinkedPtrOrderedListTest_lower_bound_same_value)
{
    const auto count = GetParam();

    using node_t = ListNodeMock<int32_t>;
    std::vector<node_t> vec;

    for(int32_t i = 0; i < count; ++i)
    {
        vec.emplace_back(42);
    }

    tinycoro::detail::LinkedPtrOrderedList<node_t> list;

    for(auto& it : vec)
    {
        list.insert(&it);
    }

    EXPECT_EQ(list.size(), vec.size());

    auto range = list.lower_bound(42);

    EXPECT_EQ(ValidateRange(range), count);
    EXPECT_TRUE(list.empty());
}