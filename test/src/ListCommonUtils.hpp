#ifndef __TINY_CORO_TEST_LIST_COMMON_TEST_HPP__
#define __TINY_CORO_TEST_LIST_COMMON_TEST_HPP__

#include <algorithm>
#include <vector>

namespace tinycoro { namespace test {

    template<typename NodeT, template<typename> class ListT>
    void ReverseCheck(const size_t count)
    {
        std::vector<NodeT> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            vec.push_back({});
        }

        ListT<NodeT> stack;
        std::ranges::for_each(vec, [&](auto& it) { stack.push(&it); });

        EXPECT_EQ(vec.size(), stack.size());

        std::for_each(vec.rbegin(), vec.rend(), [&](auto& it) { std::ignore = stack.erase(&it); });
        EXPECT_EQ(stack.size(), 0);
    }

    template<typename NodeT, template<typename> class ListT>
    void OrderCheck(const size_t count)
    {
        std::vector<NodeT> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            vec.push_back({});
        }

        ListT<NodeT> stack;
        std::ranges::for_each(vec, [&](auto& it) { stack.push(&it); });

        EXPECT_EQ(vec.size(), stack.size());

        std::ranges::for_each(vec, [&](auto& it) { std::ignore = stack.erase(&it); });
        EXPECT_EQ(stack.size(), 0);
    }

}} // namespace tinycoro::test

#endif //! __TINY_CORO_TEST_LIST_COMMON_TEST_HPP__