#include <gtest/gtest.h>

#include <tinycoro/StopSourceAwaiter.hpp>

#include "mock/CoroutineHandleMock.h"

template<typename T>
struct PromiseMock
{
    T stopSource;
};

TEST(StopSourceAwaiterTest, StopSourceAwaiterTest)
{
    tinycoro::StopSourceAwaiter ssa{};

    tinycoro::test::CoroutineHandleMock<PromiseMock<std::stop_source>> hdl;

    EXPECT_EQ(ssa.await_ready(), false);

    auto mockHdl = ssa.await_suspend(hdl);
    EXPECT_TRUE((std::same_as<decltype(mockHdl), decltype(hdl)>));

    auto stopSource = ssa.await_resume();
    EXPECT_TRUE((std::same_as<decltype(stopSource), std::stop_source>));
}

TEST(StopSourceAwaiterTest, StopTokenAwaiterTest)
{
    tinycoro::StopTokenAwaiter ssa{};

    tinycoro::test::CoroutineHandleMock<PromiseMock<std::stop_source>> hdl;

    EXPECT_EQ(ssa.await_ready(), false);

    auto mockHdl = ssa.await_suspend(hdl);
    EXPECT_TRUE((std::same_as<decltype(mockHdl), decltype(hdl)>));

    auto stopToken = ssa.await_resume();
    EXPECT_TRUE((std::same_as<decltype(stopToken), std::stop_token>));
}