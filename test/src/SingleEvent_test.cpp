#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <concepts>
#include <semaphore>

#include "mock/CoroutineHandleMock.h"

#include "tinycoro/tinycoro_all.h"

TEST(SingleEventTest, SingleEventTest_Set)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.Set(42);
    EXPECT_TRUE(singleEvent.IsSet());
}

template <typename T, typename U>
class PopAwaiterMock : tinycoro::detail::SingleLinkable<PopAwaiterMock<T, U>>
{
public:
    PopAwaiterMock(auto&, auto) { }
};

TEST(SingleEventTest, SingleEventTest_coawaitReturn)
{
    tinycoro::detail::SingleEvent<int32_t, PopAwaiterMock> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    using expectedAwaiterType = PopAwaiterMock<decltype(singleEvent), tinycoro::detail::ResumeSignalEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(SingleEventTest, SingleEventTest_await_resume)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.Set(42);
    EXPECT_TRUE(singleEvent.IsSet());

    EXPECT_TRUE(awaiter.await_ready());

    auto val = awaiter.await_resume();
    EXPECT_EQ(val, 42);
    EXPECT_FALSE(singleEvent.IsSet());

    singleEvent.Set(44);

    auto awaiter2 = singleEvent.operator co_await();

    EXPECT_TRUE(singleEvent.IsSet());
    EXPECT_TRUE(awaiter2.await_ready());

    val = awaiter2.await_resume();
    EXPECT_EQ(val, 44);
    EXPECT_FALSE(singleEvent.IsSet());
}

TEST(SingleEventTest, SingleEventTest_await_ready)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    EXPECT_FALSE(singleEvent.IsSet());
    singleEvent.Set(42);
    EXPECT_TRUE(singleEvent.IsSet());

    EXPECT_TRUE(awaiter.await_ready());
}

TEST(SingleEventTest, SingleEventTest_await_suspend)
{
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto awaiter = singleEvent.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    bool pauseCalled = false;
    auto hdl         = tinycoro::test::MakeCoroutineHdl([&pauseCalled](auto) { pauseCalled = true; });

    EXPECT_TRUE(awaiter.await_suspend(hdl));
    EXPECT_FALSE(pauseCalled);

    auto awaiter2 = singleEvent.operator co_await();

    auto hdl2 = tinycoro::test::MakeCoroutineHdl();

    auto func = [&]{std::ignore = awaiter2.await_suspend(hdl2); };
    // allow only 1 consumer
    EXPECT_THROW(func(), tinycoro::SingleEventException);

    EXPECT_FALSE(singleEvent.IsSet());
    EXPECT_TRUE(singleEvent.Set(42));
    EXPECT_TRUE(pauseCalled);
    EXPECT_FALSE(singleEvent.IsSet());

    EXPECT_TRUE(singleEvent.Set(43));
    EXPECT_TRUE(singleEvent.IsSet());

}

struct SingleNotifierMockImpl
{
    MOCK_METHOD(bool, Set, (tinycoro::ResumeCallback_t));
    MOCK_METHOD(void, Notify, (tinycoro::ENotifyPolicy));
};

struct SingleNotifierMock
{
    bool Set(auto func) { return mock->Set(func); }
    void Notify(tinycoro::ENotifyPolicy p = tinycoro::ENotifyPolicy::RESUME) { mock->Notify(p); }

    std::shared_ptr<SingleNotifierMockImpl> mock = std::make_shared<SingleNotifierMockImpl>();
};

template<typename T>
struct SingleEventMock
{
    using value_type = T;

    MOCK_METHOD(bool, Add, (void*));
};

TEST(SingleEventTest, SingleEventTest_await_suspend_noSuspend)
{
    SingleEventMock<int32_t>    mock;
    SingleNotifierMock notifier;

    tinycoro::detail::SingleEventAwaiter awaiter{mock, notifier};

    EXPECT_CALL(mock, Add(std::addressof(awaiter))).Times(1).WillOnce(testing::Return(false));
    EXPECT_CALL(*notifier.mock, Notify(tinycoro::ENotifyPolicy::RESUME)).Times(0); // no call
    EXPECT_CALL(*notifier.mock, Set).Times(2);

    auto hdl = tinycoro::test::MakeCoroutineHdl();

    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

TEST(SingleEventTest, SingleEventFunctionalTest_1)
{
    tinycoro::Scheduler            scheduler{4};
    tinycoro::SingleEvent<int32_t> singleEvent;

    auto producer = [&singleEvent]() -> tinycoro::Task<void> {
        singleEvent.Set(42);
        co_return;
    };

    auto consumer = [&singleEvent]() -> tinycoro::Task<void> {
        auto val = co_await singleEvent;
        EXPECT_EQ(val, 42);
    };

    tinycoro::AllOf(scheduler, producer(), consumer());
}

TEST(SingleEventTest, SingleEventFunctionalTest_2)
{
    // single threaded mode
    tinycoro::Scheduler            scheduler{4};
    tinycoro::SingleEvent<int32_t> singleEvent1;
    tinycoro::SingleEvent<int32_t> singleEvent2;

    auto producer = [&]() -> tinycoro::Task<void> {
        int32_t val{};
        while (val < 10)
        {
            auto lastValue = val;

            singleEvent1.Set(val + 1);
            val = co_await singleEvent2;

            EXPECT_EQ(lastValue + 2, val);
        }
    };

    auto consumer = [&]() -> tinycoro::Task<void> {
        int32_t val{-1};
        while (val < 9)
        {
            auto lastValue = val;

            val = co_await singleEvent1;
            singleEvent2.Set(val + 1);

            EXPECT_EQ(lastValue + 2, val);
        }
    };

    tinycoro::AllOf(scheduler, producer(), consumer());
}

TEST(SingleEventTest, SingleEventTest_cancel)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    tinycoro::SingleEvent<int32_t> event;

    auto receiver = [&]() -> tinycoro::TaskNIC<int32_t> {
        auto result = co_await tinycoro::Cancellable(event.Wait());
        co_return result;
    };

    auto [r1, r2] = tinycoro::AnyOf(scheduler, receiver(), tinycoro::SleepFor(clock, 100ms));

    EXPECT_FALSE(r1.has_value());
    EXPECT_TRUE(r2.has_value());
}

TEST(SingleEventTest, SingleEventTest_cancel_inline)
{
    tinycoro::SoftClock clock;

    tinycoro::SingleEvent<int32_t> event;

    auto receiver = [&]() -> tinycoro::TaskNIC<int32_t> {
        auto result = co_await tinycoro::Cancellable(event.Wait());
        co_return result;
    };

    auto [r1, r2] = tinycoro::AnyOf(receiver(), tinycoro::SleepFor(clock, 100ms));

    EXPECT_FALSE(r1.has_value());
    EXPECT_TRUE(r2.has_value());
}

struct SingleEventTimeoutTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SingleEventTimeoutTest, SingleEventTimeoutTest, testing::Values(1, 10, 100, 1000, 10000, 20000));

// THIS TEST CAN HANG!!! (fixed)
TEST_P(SingleEventTimeoutTest, SingleEventFunctionalTest_timeout_race)
{

    tinycoro::Scheduler scheduler{2};
    tinycoro::SoftClock clock;

    tinycoro::SingleEvent<uint32_t> event;
    uint32_t         doneCount{};

    // tinycoro::AutoEvent helperEvent{true};
    std::binary_semaphore sema{1};

    auto count = GetParam();

    auto SingleEventConsumer = [&]() -> tinycoro::TaskNIC<> {
        while (doneCount < count)
        {
            auto opt = co_await tinycoro::TimeoutAwait{clock, event.Wait(), 10ms};
            if (opt.has_value())
            {
                sema.release();
                // helperEvent.Set();
                doneCount++;
                //EXPECT_EQ(doneCount++, *opt);
            }
        }
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<> {
        for ([[maybe_unused]] auto it : std::ranges::views::iota(0u, count))
        {
            sema.acquire();
            // co_await helperEvent;
            [[maybe_unused]] auto succeed = event.Set(it);
            //EXPECT_TRUE(succeed);
        }

        co_return;
    };

    tinycoro::AllOf(scheduler, SingleEventConsumer(), sleep());

    EXPECT_EQ(doneCount, count);
}

// THIS TEST CAN HANG!!! (fixed)
TEST_P(SingleEventTimeoutTest, SingleEventFunctionalTest_timeout_race_auto_event)
{

    tinycoro::Scheduler scheduler{2};
    tinycoro::SoftClock clock;

    tinycoro::SingleEvent<uint32_t> event;
    uint32_t                        doneCount{};

    tinycoro::AutoEvent helperEvent{true};

    auto count = GetParam();

    auto SingleEventConsumer = [&]() -> tinycoro::TaskNIC<> {
        while (doneCount < count)
        {
            auto opt = co_await tinycoro::TimeoutAwait{clock, event.Wait(), 10ms};
            if (opt.has_value())
            {
                helperEvent.Set();
                doneCount++;
            }
        }
    };

    auto sleep = [&]() -> tinycoro::TaskNIC<> {
        for ([[maybe_unused]] auto it : std::ranges::views::iota(0u, count))
        {
            co_await helperEvent;
            [[maybe_unused]] auto succeed = event.Set(it);
            EXPECT_TRUE(succeed);
        }

        co_return;
    };

    tinycoro::AllOf(scheduler, SingleEventConsumer(), sleep());

    EXPECT_EQ(doneCount, count);
}


struct SingleEventTimeoutAllTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SingleEventTimeoutAllTest, SingleEventTimeoutAllTest, testing::Values(1, 10, 100, 200));

TEST_P(SingleEventTimeoutAllTest, SingleEventFunctionalTest_all_timeout)
{
    tinycoro::SoftClock clock;

    const auto count = GetParam();

    tinycoro::SingleEvent<int32_t> event;
    int32_t                        doneCount{};
    tinycoro::AutoEvent            helperEvent{true};

    auto SingleEventConsumer = [&]() -> tinycoro::TaskNIC<> {
        co_await helperEvent;

        auto opt = co_await tinycoro::TimeoutAwait{clock, event.Wait(), 1ms};
        EXPECT_FALSE(opt.has_value());
        doneCount++;

        helperEvent.Set();
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(SingleEventConsumer());
    }

    tinycoro::AllOf(std::move(tasks));

    EXPECT_EQ(doneCount, count);
}

TEST(SingleEventTimeoutTest, SingleEventFunctionalTest_timeout_one_task)
{

    tinycoro::Scheduler scheduler{2};
    tinycoro::SoftClock clock;

    tinycoro::SingleEvent<int32_t> event;

    auto SingleEventConsumer = [&]() -> tinycoro::TaskNIC<> {
        auto opt = co_await tinycoro::TimeoutAwait{clock, event.Wait(), 10ms};
        EXPECT_FALSE(opt.has_value());
    };

    tinycoro::AllOf(scheduler, SingleEventConsumer());
}