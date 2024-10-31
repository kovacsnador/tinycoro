#include <gtest/gtest.h>

#include <concepts>

#include "mock/CoroutineHandleMock.h"

#include "tinycoro/tinycoro_all.h"

TEST(SingleEventTest, SingleEventTest_Set)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.SetValue(42);
    EXPECT_TRUE(singleEvent.IsSet());
}

template<typename, typename>
struct AwaiterMock
{
    AwaiterMock(auto&, auto) {}
};

TEST(SingleEventTest, SingleEventTest_coawaitReturn)
{
    tinycoro::detail::SingleEvent<int32_t, AwaiterMock> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(singleEvent), tinycoro::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(SingleEventTest, SingleEventTest_await_resume)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.SetValue(42);
    EXPECT_TRUE(singleEvent.IsSet());

    auto val = awaiter.await_resume();
    EXPECT_EQ(val, 42);
    EXPECT_FALSE(singleEvent.IsSet());

    singleEvent.SetValue(44);
    EXPECT_TRUE(singleEvent.IsSet());

    val = awaiter.await_resume();
    EXPECT_EQ(val, 44);
    EXPECT_FALSE(singleEvent.IsSet());
}

TEST(SingleEventTest, SingleEventTest_await_ready)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.SetValue(42);
    EXPECT_TRUE(singleEvent.IsSet());

    EXPECT_TRUE(awaiter.await_ready());
}

TEST(SingleEventTest, SingleEventTest_await_suspend)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    bool pauseCalled = false;
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;
    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&pauseCalled]() { pauseCalled = true; });

    awaiter.await_suspend(hdl);
    EXPECT_FALSE(pauseCalled);

    // allow only 1 consumer
    EXPECT_THROW(awaiter.await_suspend(hdl), tinycoro::SingleEventException);

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.SetValue(42);
    EXPECT_TRUE(pauseCalled);
    EXPECT_TRUE(singleEvent.IsSet());
}

TEST(SingleEventTest, SingleEventFunctionalTest)
{
    tinycoro::Scheduler scheduler{4};
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto producer = [&singleEvent]() -> tinycoro::Task<void>
    {
        singleEvent.SetValue(42);
        co_return;
    };

    auto consumer = [&singleEvent]() -> tinycoro::Task<void>
    {
        auto val = co_await singleEvent;
        EXPECT_EQ(val, 42);
    };

    tinycoro::GetAll(scheduler, producer(), consumer());
}