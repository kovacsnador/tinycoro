#include <gtest/gtest.h>

#include <tinycoro/StopSourceAwaiter.hpp>

#include "mock/CoroutineHandleMock.h"

template<typename T>
struct PromiseMock
{
    T stopSource;

    auto& StopSource() noexcept
    {
        return stopSource;
    }

    void CreateSharedState(bool) { }
};

TEST(StopSourceAwaiterTest, StopSourceAwaiterTest)
{
    tinycoro::StopSourceAwaiter ssa{};

    tinycoro::test::CoroutineHandleMock<PromiseMock<std::stop_source>> hdl;

    EXPECT_EQ(ssa.await_ready(), false);

    EXPECT_FALSE(ssa.await_suspend(hdl));

    auto stopSource = ssa.await_resume();
    EXPECT_TRUE((std::same_as<decltype(stopSource), std::stop_source>));

    EXPECT_TRUE(stopSource.stop_possible());
}

template<typename T>
struct PromiseMockNoState
{
    T stopSource{std::nostopstate};

    auto& StopSource() noexcept
    {
        stopSource = {};
        return stopSource;
    }

    void CreateSharedState(bool) { }
};

TEST(StopSourceAwaiterTest, StopSourceAwaiterTest_nostate)
{
    tinycoro::StopSourceAwaiter ssa{};

    tinycoro::test::CoroutineHandleMock<PromiseMockNoState<std::stop_source>> hdl;

    EXPECT_EQ(ssa.await_ready(), false);

    EXPECT_FALSE(ssa.await_suspend(hdl));

    auto stopSource = ssa.await_resume();
    EXPECT_TRUE((std::same_as<decltype(stopSource), std::stop_source>));

    EXPECT_TRUE(stopSource.stop_possible());
}

TEST(StopSourceAwaiterTest, StopTokenAwaiterTest)
{
    tinycoro::StopTokenAwaiter ssa{};

    tinycoro::test::CoroutineHandleMock<PromiseMock<std::stop_source>> hdl;

    EXPECT_EQ(ssa.await_ready(), false);

    EXPECT_FALSE(ssa.await_suspend(hdl));

    auto stopToken = ssa.await_resume();
    EXPECT_TRUE((std::same_as<decltype(stopToken), std::stop_token>));

    EXPECT_TRUE(stopToken.stop_possible());
}

TEST(StopSourceAwaiterTest, StopTokenAwaiterTest_nostate)
{
    tinycoro::StopTokenAwaiter ssa{};

    tinycoro::test::CoroutineHandleMock<PromiseMockNoState<std::stop_source>> hdl;

    EXPECT_EQ(ssa.await_ready(), false);

    EXPECT_FALSE(ssa.await_suspend(hdl));

    auto stopToken = ssa.await_resume();
    EXPECT_TRUE((std::same_as<decltype(stopToken), std::stop_token>));

    EXPECT_TRUE(stopToken.stop_possible());
}

TEST(StopSourceAwaiterTest, test_this_coro_functions)
{
    EXPECT_TRUE((std::same_as<decltype(tinycoro::this_coro::stop_source()), tinycoro::StopSourceAwaiter<>>));
    EXPECT_TRUE((std::same_as<decltype(tinycoro::this_coro::stop_token()), tinycoro::StopTokenAwaiter<>>));
}