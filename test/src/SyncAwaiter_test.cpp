#include <gtest/gtest.h>

#include <future>
#include <tuple>
#include <stop_token>

#include <tinycoro/SyncAwait.hpp>
#include <tinycoro/Task.hpp>
#include <tinycoro/Scheduler.hpp>
#include <tinycoro/CancellableSuspend.hpp>
#include <tinycoro/StopSourceAwaiter.hpp>

#include "mock/CoroutineHandleMock.h"

template <typename T, typename... Ts>
concept AllSame = (std::same_as<T, Ts> && ...);

struct SchedulerMock
{
    template <typename CoroTaskT>
    auto Enqueue([[maybe_unused]] CoroTaskT&& t)
    {
        auto taks = std::move(t);
        using ValueT = CoroTaskT::promise_type::value_type;
        std::promise<ValueT> promise;

        if constexpr (AllSame<void, ValueT>)
        {
            promise.set_value();
        }
        else
        {
            promise.set_value({});
        }
        return promise.get_future();
    }

    template <typename... Args>
        requires (sizeof...(Args) > 1)
    auto Enqueue([[maybe_unused]] Args&&... args)
    {
        auto tupleTask = std::make_tuple(std::forward<Args>(args)...);

        auto promises = std::make_tuple(std::promise<typename Args::promise_type::value_type>{}...);
        std::apply(
            []<typename... Ts>(Ts&&... ts) {
                auto setPromise = [](auto& p) {
                    using ResultType = std::decay_t<decltype(p.get_future().get())>;

                    if constexpr (AllSame<void, ResultType>)
                    {
                        p.set_value();
                    }
                    else
                    {
                        p.set_value({});
                    }
                };

                (setPromise(ts), ...);
            },
            promises);
        return std::apply([]<typename... Ts>(Ts&&... ts) { return std::make_tuple(ts.get_future()...); }, promises);
    }
};

struct SyncAwaiterTest : testing::Test
{
    void SetUp() override
    {
        hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([this] { resumerCalled = true; });
    }

protected:
    bool resumerCalled{false};
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<void>> hdl;

    SchedulerMock schedulerMock;
};

TEST_F(SyncAwaiterTest, SyncAwaiterTest_voidTask)
{
    auto task    = []() -> tinycoro::Task<void> { co_return; };
    auto awaiter = tinycoro::SyncAwait(schedulerMock, task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_FALSE(resumerCalled);
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_TRUE(resumerCalled);
    EXPECT_NO_THROW(awaiter.await_resume());
}

TEST_F(SyncAwaiterTest, SyncAwaiterTest_intTask)
{
    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto awaiter = tinycoro::SyncAwait(schedulerMock, task(), task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_FALSE(resumerCalled);
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_TRUE(resumerCalled);

    auto results = awaiter.await_resume();

    EXPECT_EQ(std::get<0>(results), 0);
    EXPECT_EQ(std::get<1>(results), 0);
    EXPECT_EQ(std::get<2>(results), 0);
}

TEST_F(SyncAwaiterTest, AnyOfAwait_intTask)
{
    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto awaiter = tinycoro::AnyOfAwait(schedulerMock, task(), task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_FALSE(resumerCalled);
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_TRUE(resumerCalled);

    auto results = awaiter.await_resume();

    EXPECT_EQ(std::get<0>(results), 0);
    EXPECT_EQ(std::get<1>(results), 0);
    EXPECT_EQ(std::get<2>(results), 0);
}

TEST_F(SyncAwaiterTest, AnyOfAwaitStopSource_intTask)
{
    std::stop_source ss;

    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto awaiter = tinycoro::AnyOfStopSourceAwait(schedulerMock, ss, task(), task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_FALSE(resumerCalled);
    EXPECT_FALSE(ss.stop_requested());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_TRUE(resumerCalled);
    EXPECT_TRUE(ss.stop_requested());

    auto results = awaiter.await_resume();

    EXPECT_EQ(std::get<0>(results), 0);
    EXPECT_EQ(std::get<1>(results), 0);
    EXPECT_EQ(std::get<2>(results), 0);
}

tinycoro::Task<std::string> AsyncAwaiterTest1(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<std::string> { co_return "123"; };
    auto task2 = []() -> tinycoro::Task<std::string> { co_return "456"; };
    auto task3 = []() -> tinycoro::Task<std::string> { co_return "789"; };

    auto tupleResult = co_await tinycoro::SyncAwait(scheduler, task1(), task2(), task3());

    // tuple accumulate
    co_return std::apply(
        []<typename... Ts>(Ts&&... ts) {
            std::string result;
            (result.append(std::forward<Ts>(ts)), ...);
            return result;
        },
        tupleResult);
}

TEST(AsyncAwaiterTest1, AsyncAwaiterTest1)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto future = scheduler.Enqueue(AsyncAwaiterTest1(scheduler));
    EXPECT_EQ(std::string{"123456789"}, future.get());
}

tinycoro::Task<void> AsyncAwaiterTest2(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<void> { co_return; };
    auto task2 = []() -> tinycoro::Task<void> { co_return; };
    auto task3 = []() -> tinycoro::Task<void> { co_return; };

    co_await tinycoro::SyncAwait(scheduler, task1(), task2(), task3());
}

TEST(AsyncAwaiterTest2, AsyncAwaiterTest2)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto future = scheduler.Enqueue(AsyncAwaiterTest2(scheduler));
    EXPECT_NO_THROW(future.get());
}

tinycoro::Task<void> AnyOfCoAwaitTest1(auto& scheduler)
{
    auto now = std::chrono::system_clock::now();

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {

        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    auto results = co_await tinycoro::AnyOfAwait(scheduler, task1(100ms), task1(2s), task1(3s));

    EXPECT_TRUE(std::get<0>(results) > 0);
    EXPECT_TRUE(std::get<1>(results) > 0);
    EXPECT_TRUE(std::get<2>(results) > 0);

    EXPECT_TRUE(std::chrono::system_clock::now() - now < 500ms);
}

TEST(AnyOfCoAwaitTest1, AnyOfCoAwaitTest1)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto future = scheduler.Enqueue(AnyOfCoAwaitTest1(scheduler));
    EXPECT_NO_THROW(tinycoro::GetAll(future));
}

tinycoro::Task<void> AnyOfCoAwaitTest2(auto& scheduler)
{
    auto now = std::chrono::system_clock::now();

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    auto stopSource = co_await tinycoro::StopSourceAwaiter{};

    auto results = co_await tinycoro::AnyOfStopSourceAwait(scheduler, stopSource, task1(100ms), task1(2s), task1(3s));

    EXPECT_TRUE(std::get<0>(results) > 0);
    EXPECT_TRUE(std::get<1>(results) > 0);
    EXPECT_TRUE(std::get<2>(results) > 0);

    EXPECT_TRUE(std::chrono::system_clock::now() - now < 500ms);
}

TEST(AnyOfCoAwaitTest2, AnyOfCoAwaitTest2)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto future = scheduler.Enqueue(AnyOfCoAwaitTest2(scheduler));
    EXPECT_NO_THROW(tinycoro::GetAll(future));
}