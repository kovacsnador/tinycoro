#include <gtest/gtest.h>

#include <tinycoro/Task.hpp>
#include <tinycoro/CancellableSuspend.hpp>

#include "mock/CoroutineHandleMock.h"

TEST(CancellableSuspentTest, CancellableSuspentTest)
{
    tinycoro::CancellableSuspend suspend{};

    auto hdl = tinycoro::test::MakeCoroutineHdl();
    
    suspend.await_suspend(hdl);

    EXPECT_TRUE(hdl.promise().pauseHandler->IsCancellable());
}