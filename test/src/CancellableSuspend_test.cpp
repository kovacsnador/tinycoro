#include <gtest/gtest.h>

#include <tinycoro/Task.hpp>
#include <tinycoro/CancellableSuspend.hpp>

#include "mock/CoroutineHandleMock.h"


TEST(CancellableSuspentTest, CancellableSuspentTest_value)
{
    int32_t val{42};
    tinycoro::CancellableSuspend suspend{val};

    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;

    suspend.await_suspend(hdl);

    EXPECT_TRUE(hdl.promise().cancellable);
    EXPECT_EQ(hdl.promise().ReturnValue(), 42);
}

TEST(CancellableSuspentTest, CancellableSuspentTest_void)
{
    tinycoro::CancellableSuspend<void> suspend{};

    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<void>> hdl;
    
    suspend.await_suspend(hdl);

    EXPECT_TRUE(hdl.promise().cancellable);
}