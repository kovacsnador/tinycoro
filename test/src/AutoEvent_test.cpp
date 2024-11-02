#include <gtest/gtest.h>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(AutoEventTest, AutoEventTest_set)
{
    tinycoro::AutoEvent event;
    
    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());
}

TEST(AutoEventTest, AutoEventTest_constructor)
{
    tinycoro::AutoEvent event{true};
    
    EXPECT_TRUE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());
}

template<typename, typename>
struct AwaiterMock
{
    AwaiterMock(auto&, auto) {}

    AwaiterMock* next{nullptr};
};

TEST(AutoEventTest, AutoEventTest_coawaitReturn)
{
    tinycoro::detail::AutoEvent<AwaiterMock> event;

    auto awaiter = event.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(event), tinycoro::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(AutoEventTest, SingleEventTest_await_ready)
{
    tinycoro::AutoEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());

    EXPECT_TRUE(awaiter.await_ready());
    
    // auto reset
    EXPECT_FALSE(event.IsSet());
    EXPECT_FALSE(awaiter.await_ready());
}

TEST(AutoEventTest, SingleEventTest_await_suspend)
{
    tinycoro::AutoEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    bool pauseCalled = false;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseCalled]() { pauseCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), std::noop_coroutine());

    EXPECT_FALSE(pauseCalled);

    EXPECT_FALSE(event.IsSet());
    event.Set();

    // auto reset
    EXPECT_FALSE(event.IsSet());
    EXPECT_TRUE(pauseCalled);
}

TEST(AutoEventTest, SingleEventTest_await_suspend_preset)
{
    tinycoro::AutoEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    event.Set();

    bool pauseCalled = false;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseCalled]() { pauseCalled = true; });

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);

    EXPECT_FALSE(pauseCalled);

    EXPECT_FALSE(event.IsSet());
    event.Set();

    // auto reset
    EXPECT_TRUE(event.IsSet());

    // no pause pauseCalled, immediately resumed coroutine
    EXPECT_FALSE(pauseCalled);
}