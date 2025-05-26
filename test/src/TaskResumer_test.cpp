#include <gtest/gtest.h>

#include <iterator>

#include <tinycoro/TaskResumer.hpp>

struct ContinuationMock
{
    ContinuationMock* child{nullptr};
};

TEST(TaskResumerTest, TaskResumerTest_FindContinuationAndConnect)
{
    ContinuationMock p1;
    ContinuationMock p2;
    ContinuationMock p3;

    p1.child = std::addressof(p2);
    p2.child = std::addressof(p3);

    auto promise = tinycoro::detail::TaskResumer::FindContinuation(&p1);
    EXPECT_EQ(std::addressof(p3), promise);
}

TEST(TaskResumerTest, TaskResumerTest_FindContinuationAndConnect_single_root)
{
    ContinuationMock p1;

    auto promise = tinycoro::detail::TaskResumer::FindContinuation(&p1);
    EXPECT_EQ(std::addressof(p1), promise);

    EXPECT_EQ(p1.child, nullptr);
}