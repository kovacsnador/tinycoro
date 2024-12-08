#include <gtest/gtest.h>

#include <tinycoro/Task.hpp>
#include <tinycoro/CancellableSuspend.hpp>

#include "mock/CoroutineHandleMock.h"


TEST(CancellableSuspentTest, CancellableSuspentTest_value)
{
    int32_t val{42};
    tinycoro::CancellableSuspend suspend{val};

    auto hdl = tinycoro::test::MakeCoroutineHdl<int32_t>([]{});

    suspend.await_suspend(hdl);

    EXPECT_TRUE(hdl.promise().pauseHandler->IsCancellable());
    EXPECT_EQ(hdl.promise().ReturnValue(), 42);
}

TEST(CancellableSuspentTest, CancellableSuspentTest_void)
{
    tinycoro::CancellableSuspend<void> suspend{};

    auto hdl = tinycoro::test::MakeCoroutineHdl([]{});
    
    suspend.await_suspend(hdl);

    EXPECT_TRUE(hdl.promise().pauseHandler->IsCancellable());
}