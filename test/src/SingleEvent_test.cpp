#include <gtest/gtest.h>
#include <gmock/gmock.h>

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
class AwaiterMock
{
public:
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
    auto hdl = tinycoro::test::MakeCoroutineHdl([&pauseCalled]() { pauseCalled = true; });

    awaiter.await_suspend(hdl);
    EXPECT_FALSE(pauseCalled);

    auto awaiter2 = singleEvent.operator co_await();

    auto hdl2 = tinycoro::test::MakeCoroutineHdl([]{});

    // allow only 1 consumer
    EXPECT_THROW(awaiter2.await_suspend(hdl2), tinycoro::SingleEventException);

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.SetValue(42);
    EXPECT_TRUE(pauseCalled);
    EXPECT_TRUE(singleEvent.IsSet());
}

struct SingleNotifierMockImpl
{
    MOCK_METHOD(bool, Set, (std::function<void()>));
    MOCK_METHOD(void, Notify, ());
};

struct SingleNotifierMock
{
    bool Set(auto func){ return mock->Set(func); } 
    void Notify(){ mock->Notify(); } 

    std::shared_ptr<SingleNotifierMockImpl> mock = std::make_shared<SingleNotifierMockImpl>();
};

struct SingleEventMock
{
    MOCK_METHOD(bool, Add, (void*));
};

TEST(SingleEventTest, SingleEventTest_await_suspend_noSuspend)
{
    SingleEventMock mock;
    SingleNotifierMock notifier;

    tinycoro::detail::SingleEventAwaiter awaiter{mock, notifier};

    EXPECT_CALL(mock, Add(std::addressof(awaiter))).Times(1).WillOnce(testing::Return(false));
    EXPECT_CALL(*notifier.mock, Notify()).Times(0); // no call
    EXPECT_CALL(*notifier.mock, Set).Times(2);

    auto hdl = tinycoro::test::MakeCoroutineHdl([]{});

    EXPECT_EQ(awaiter.await_suspend(hdl), hdl);
}

TEST(SingleEventTest, SingleEventFunctionalTest_1)
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

TEST(SingleEventTest, SingleEventFunctionalTest_2)
{
    // single threaded mode
    tinycoro::Scheduler scheduler{4};
    tinycoro::SingleEvent<int32_t> singleEvent1;
    tinycoro::SingleEvent<int32_t> singleEvent2;

    auto producer = [&]() -> tinycoro::Task<void>
    {
        int32_t val{};
        while(val < 10)
        {
            auto lastValue = val;

            singleEvent1.SetValue(val + 1);
            val = co_await singleEvent2;
            
            EXPECT_EQ(lastValue + 2, val);
        }
    };

    auto consumer = [&]() -> tinycoro::Task<void>
    {
        int32_t val{-1};
        while(val < 9)
        {
            auto lastValue = val;

            val = co_await singleEvent1;
            singleEvent2.SetValue(val + 1);

            EXPECT_EQ(lastValue + 2, val);
        }
    };

    tinycoro::GetAll(scheduler, producer(), consumer());
}