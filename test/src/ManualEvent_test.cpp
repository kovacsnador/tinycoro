#include <gtest/gtest.h>

#include <ranges>
#include <algorithm>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

TEST(ManualEventTest, ManualEventTest_set_reset)
{
    tinycoro::ManualEvent event;

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());

    event.Reset();

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());
}

template <typename, typename>
class PopAwaiterMock
{
public:
    PopAwaiterMock(auto&, auto) { }

    void Notify() const noexcept { }

    PopAwaiterMock* next{nullptr};
};

TEST(ManualEventTest, ManualEventTest_coawaitReturn)
{
    tinycoro::detail::ManualEvent<PopAwaiterMock> event;

    auto awaiter = event.operator co_await();

    using expectedAwaiterType = PopAwaiterMock<decltype(event), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(ManualEventTest, ManualEventTest_await_ready)
{
    tinycoro::ManualEvent event;

    auto awaiter = event.operator co_await();

    EXPECT_FALSE(awaiter.await_ready());

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(event.IsSet());

    EXPECT_TRUE(awaiter.await_ready());

    event.Reset();
    EXPECT_FALSE(awaiter.await_ready());
}

TEST(ManualEventTest, ManualEventTest_already_ready_await_ready)
{
    tinycoro::ManualEvent event;

    auto awaiter = event.operator co_await();

    event.Set();
    EXPECT_TRUE(awaiter.await_ready());
}

TEST(ManualEventTest, ManualEventTest_await_suspend)
{
    tinycoro::ManualEvent event;

    auto awaiter = event.operator co_await();

    bool pauseResumerCalled = false;
    auto cb = tinycoro::test::MakePauseResumeCallback<bool, true>(&pauseResumerCalled);
    auto hdl = tinycoro::test::MakeCoroutineHdl(cb);

    event.Set();
    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

TEST(ManualEventTest, ManualEventTest_await_suspend_singleConsumer)
{
    tinycoro::ManualEvent event;

    auto awaiter1 = event.operator co_await();

    EXPECT_FALSE(awaiter1.await_ready());

    bool pauseResumerCalled = false;
    auto cb = tinycoro::test::MakePauseResumeCallback<bool, true>(&pauseResumerCalled);
    auto hdl = tinycoro::test::MakeCoroutineHdl(cb);

    EXPECT_TRUE(awaiter1.await_suspend(hdl));
    EXPECT_FALSE(pauseResumerCalled);

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_TRUE(pauseResumerCalled);
    EXPECT_TRUE(event.IsSet());
}

TEST(ManualEventTest, ManualEventTest_await_suspend_multiConsumer)
{
    tinycoro::ManualEvent event;

    auto awaiter1 = event.operator co_await();
    EXPECT_FALSE(awaiter1.await_ready());

    auto awaiter2 = event.operator co_await();
    EXPECT_FALSE(awaiter2.await_ready());

    auto awaiter3 = event.operator co_await();
    EXPECT_FALSE(awaiter3.await_ready());

    size_t pauseResumerCalled   = 0;
    //auto   pauseResumerCallback = [&pauseResumerCalled]() { ++pauseResumerCalled; };

    auto makeHdl = [&]() {

        auto cb = [](void* valPtr, void*, void*) {
            auto* val = static_cast<size_t*>(valPtr);
            (*val)++;
        };

        return tinycoro::test::MakeCoroutineHdl(tinycoro::PauseHandlerCallbackT{cb, &pauseResumerCalled, nullptr, nullptr});
    };

    EXPECT_TRUE(awaiter1.await_suspend(makeHdl()));
    EXPECT_TRUE(awaiter2.await_suspend(makeHdl()));
    EXPECT_TRUE(awaiter3.await_suspend(makeHdl()));

    EXPECT_EQ(pauseResumerCalled, 0);

    EXPECT_FALSE(event.IsSet());
    event.Set();
    EXPECT_EQ(pauseResumerCalled, 3);
    EXPECT_TRUE(event.IsSet());
}

struct ManualEventTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(ManualEventTest, ManualEventTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(ManualEventTest, ManualEventFunctionalTest)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{4};

    tinycoro::ManualEvent event;

    std::atomic<size_t> globalCount{};

    auto trigger = [&]() -> tinycoro::Task<void> {
        event.Set();
        co_return;
    };

    auto listener = [&]() -> tinycoro::Task<void> {
        co_await event;
        ++globalCount;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(listener());
    }
    tasks.push_back(trigger());

    tinycoro::GetAll(scheduler, std::move(tasks));

    EXPECT_EQ(globalCount, count);
}

TEST_P(ManualEventTest, ManualEventFunctionalTest_preSet)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{4};

    tinycoro::ManualEvent event;

    std::atomic<size_t> globalCount{};

    auto listener = [&]() -> tinycoro::Task<void> {
        co_await event;
        ++globalCount;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(listener());
    }

    // preset the event
    event.Set();

    tinycoro::GetAll(scheduler, std::move(tasks));

    EXPECT_EQ(globalCount, count);
}

TEST_P(ManualEventTest, ManualEventTest_cancel)
{
    const auto          count = GetParam();
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    tinycoro::ManualEvent event;

    auto task = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::Cancellable(event.Wait());
        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
    }

    auto results = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(results.size(), count + 1);
    EXPECT_EQ(results[0].value(), 44);

    for (size_t i = 1; i < results.size(); ++i)
    {
        EXPECT_FALSE(results[i].has_value());
    }
}

TEST_P(ManualEventTest, ManualEventTest_set_reset_cancel_custom_cache_size)
{
    const auto          count = GetParam();
    tinycoro::CustomScheduler<64> scheduler;
    tinycoro::SoftClock clock;

    tinycoro::ManualEvent event{true};

    std::atomic<size_t> taskCount{};

    auto task = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::Cancellable(event.Wait());
        event.Reset();

        co_await tinycoro::SleepFor(clock, 1ms);
        event.Set();

        ++taskCount;

        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
    }

    auto results = tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(results.size(), count + 1);
    EXPECT_EQ(results[0].value(), 44);

    auto finished = std::ranges::count_if(results | std::views::drop(1), [](const auto& it) { return it.has_value(); });

    EXPECT_EQ(finished, taskCount);
}

TEST_P(ManualEventTest, ManualEventTest_set_reset_cancel_inline)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock;

    tinycoro::ManualEvent event{true};

    std::atomic<size_t> taskCount{};

    auto task = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::Cancellable(event.Wait());
        event.Reset();

        co_await tinycoro::SleepFor(clock, 1ms);
        event.Set();

        ++taskCount;

        co_return 42;
    };

    auto sleep = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.reserve(count + 1);
    tasks.emplace_back(sleep());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task());
    }

    auto results = tinycoro::AnyOfInline(std::move(tasks));

    EXPECT_EQ(results.size(), count + 1);
    EXPECT_EQ(results[0].value(), 44);

    auto finished = std::ranges::count_if(results | std::views::drop(1), [](const auto& it) { return it.has_value(); });

    EXPECT_EQ(finished, taskCount);
}