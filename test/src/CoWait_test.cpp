#include <gtest/gtest.h>

#include <future>
#include <tuple>
#include <stop_token>
#include <array>

#include <tinycoro/tinycoro_all.h>

#include "mock/CoroutineHandleMock.h"

template <typename T, typename... Ts>
concept AllSame = (std::same_as<T, Ts> && ...);

struct SchedulerMock
{
    template <tinycoro::EOwnPolicy ownPolicy = tinycoro::EOwnPolicy::OWNER,
              template <typename> class FutureStateT = std::promise,
              typename FinishCallbackT               = tinycoro::detail::OnTaskFinishCallbackWrapper,
              tinycoro::concepts::NonIterable CoroTaskT>
    auto Enqueue([[maybe_unused]] CoroTaskT&& t)
    {
        auto task    = std::move(t);
        using ValueT = CoroTaskT::promise_type::value_type;
        std::promise<typename tinycoro::detail::FutureReturnT<ValueT>::value_type> promise;

        if constexpr (AllSame<std::optional<tinycoro::VoidType>, ValueT>)
        {
            promise.set_value(tinycoro::VoidType{});
        }
        else
        {
            promise.set_value({});
        }
        return promise.get_future();
    }

    template <tinycoro::EOwnPolicy ownPolicy         = tinycoro::EOwnPolicy::OWNER,
              template <typename> class FutureStateT = std::promise,
              typename FinishCallbackT               = tinycoro::detail::OnTaskFinishCallbackWrapper,
              tinycoro::concepts::Iterable ContainerT>
    auto Enqueue(ContainerT&& c)
    {
        using ValueT = std::decay_t<ContainerT>::value_type::promise_type::value_type;

        using value_t = typename tinycoro::detail::FutureReturnT<ValueT>::value_type;

        std::vector<std::future<value_t>> futures;

        for (auto&& it : c)
        {
            [[maybe_unused]] auto task = std::move(it);

            std::promise<value_t> promise;

            if constexpr (AllSame<std::optional<tinycoro::VoidType>, value_t>)
            {
                promise.set_value(tinycoro::VoidType{});
            }
            else
            {
                promise.set_value(0);
            }
            futures.push_back(promise.get_future());
        }

        return futures;
    }

    template <tinycoro::EOwnPolicy ownPolicy         = tinycoro::EOwnPolicy::OWNER,
              template <typename> class FutureStateT = std::promise,
              typename FinishCallbackT               = tinycoro::detail::OnTaskFinishCallbackWrapper,
              tinycoro::concepts::NonIterable... Args>
        requires (sizeof...(Args) > 1)
    auto Enqueue([[maybe_unused]] Args&&... args)
    {
        auto tupleTask = std::make_tuple(std::forward<Args>(args)...);

        auto promises
            = std::make_tuple(std::promise<typename tinycoro::detail::FutureReturnT<typename Args::promise_type::value_type>::value_type>{}...);
        std::apply(
            []<typename... Ts>(Ts&&... ts) {
                auto setPromise = [](auto& p) {
                    using ResultType = std::decay_t<decltype(p.get_future().get())>;

                    if constexpr (AllSame<std::optional<tinycoro::VoidType>, ResultType>)
                    {
                        p.set_value(tinycoro::VoidType{});
                    }
                    else
                    {
                        p.set_value(0);
                    }
                };

                (setPromise(ts), ...);
            },
            promises);

        return std::apply([]<typename... Ts>(Ts&&... ts) { return std::make_tuple(ts.get_future()...); }, promises);
    }
};

struct AllOfAwaiterTest : testing::Test
{

protected:
    tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<void>> hdl = tinycoro::test::MakeCoroutineHdl();

    SchedulerMock schedulerMock;
};

TEST_F(AllOfAwaiterTest, AllOfAwaiterTest_voidTask)
{
    auto task    = []() -> tinycoro::Task<void> { co_return; };
    auto awaiter = tinycoro::AllOfAwait(schedulerMock, task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_NO_THROW(awaiter.await_resume());

    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), void>));
}

TEST_F(AllOfAwaiterTest, AllOfAwaiterTest_vector_voidTask)
{
    auto task = []() -> tinycoro::Task<void> { co_return; };

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task());
    tasks.push_back(task());

    auto awaiter = tinycoro::AllOfAwait(schedulerMock, tasks);

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_NO_THROW(awaiter.await_resume());

    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), void>));
}

TEST_F(AllOfAwaiterTest, AllOfAwaiterTest_intTask)
{
    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto awaiter = tinycoro::AllOfAwait(schedulerMock, task(), task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));

    auto [r1, r2, r3] = awaiter.await_resume();

    EXPECT_EQ(r1, 0);
    EXPECT_EQ(r2, 0);
    EXPECT_EQ(r3, 0);

    using return_t = tinycoro::detail::FutureTypeGetter<int32_t, tinycoro::unsafe::Promise>::futureReturn_t;
    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), std::tuple<return_t, return_t, return_t>>));
}

TEST_F(AllOfAwaiterTest, AllOfAwaiterTest_array_intTask)
{
    auto task = []() -> tinycoro::Task<int32_t> { co_return 0; };

    std::array<tinycoro::Task<int32_t>, 3> tasks;
    tasks[0] = task();
    tasks[1] = task();
    tasks[2] = task();

    auto awaiter = tinycoro::AllOfAwait(schedulerMock, tasks);

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));

    auto results = awaiter.await_resume();

    EXPECT_EQ(results[0], 0);
    EXPECT_EQ(results[1], 0);
    EXPECT_EQ(results[2], 0);

    using return_t = tinycoro::detail::FutureTypeGetter<int32_t, tinycoro::unsafe::Promise>::futureReturn_t;
    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), std::vector<return_t>>));
}

TEST_F(AllOfAwaiterTest, AnyOfAwait_intTask)
{
    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto awaiter = tinycoro::AnyOfAwait(schedulerMock, task(), task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));

    auto results = awaiter.await_resume();

    EXPECT_EQ(std::get<0>(results), 0);
    EXPECT_EQ(std::get<1>(results), 0);
    EXPECT_EQ(std::get<2>(results), 0);

    using return_t = tinycoro::detail::FutureTypeGetter<int32_t, tinycoro::unsafe::Promise>::futureReturn_t;
    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), std::tuple<return_t, return_t, return_t>>));
}

TEST_F(AllOfAwaiterTest, AnyOfAwaitStopSource_intTask)
{
    std::stop_source ss;

    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto awaiter = tinycoro::AnyOfAwait(schedulerMock, ss, task(), task(), task());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_FALSE(ss.stop_requested());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_TRUE(ss.stop_requested());

    auto results = awaiter.await_resume();

    EXPECT_EQ(std::get<0>(results), 0);
    EXPECT_EQ(std::get<1>(results), 0);
    EXPECT_EQ(std::get<2>(results), 0);

    using return_t = tinycoro::detail::FutureTypeGetter<int32_t, tinycoro::unsafe::Promise>::futureReturn_t;
    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), std::tuple<return_t, return_t, return_t>>));
}

TEST_F(AllOfAwaiterTest, AnyOfAwait_mixedTask)
{
    std::stop_source ss;

    auto task    = []() -> tinycoro::Task<int32_t> { co_return 0; };
    auto task_double    = []() -> tinycoro::Task<double> { co_return 0.0; };
    auto task_bool    = []() -> tinycoro::Task<bool> { co_return false; };
    auto awaiter = tinycoro::AnyOfAwait(schedulerMock, ss, task(), task_double(), task_bool());

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_FALSE(ss.stop_requested());
    EXPECT_NO_THROW(awaiter.await_suspend(hdl));
    EXPECT_TRUE(ss.stop_requested());

    auto results = awaiter.await_resume();

    EXPECT_EQ(std::get<0>(results), 0);
    EXPECT_EQ(std::get<1>(results), 0.0);
    EXPECT_EQ(std::get<2>(results), false);

    using return_t = tinycoro::detail::FutureTypeGetter<int32_t, tinycoro::unsafe::Promise>::futureReturn_t;
    using return_double_t = tinycoro::detail::FutureTypeGetter<double, tinycoro::unsafe::Promise>::futureReturn_t;
    using return_bool_t = tinycoro::detail::FutureTypeGetter<bool, tinycoro::unsafe::Promise>::futureReturn_t;

    EXPECT_TRUE((std::same_as<decltype(awaiter.await_resume()), std::tuple<return_t, return_double_t, return_bool_t>>));
}

tinycoro::Task<std::string> AsyncAwaiterTest1(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<std::string> { co_return "123"; };
    auto task2 = []() -> tinycoro::Task<std::string> { co_return "456"; };
    auto task3 = []() -> tinycoro::Task<std::string> { co_return "789"; };

    auto tupleResult = co_await tinycoro::AllOfAwait(scheduler, task1(), task2(), task3());

    // tuple accumulate
    co_return std::apply(
        []<typename... Ts>(Ts&&... ts) {
            std::string result;
            (result.append(*ts), ...);
            return result;
        },
        tupleResult);
}

TEST(AsyncAwaiterTest1, AsyncAwaiterTest1)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto future = scheduler.Enqueue(AsyncAwaiterTest1(scheduler));
    EXPECT_EQ(std::string{"123456789"}, future.get().value());
}

tinycoro::Task<void> AsyncAwaiterTest2(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<void> { co_return; };
    auto task2 = []() -> tinycoro::Task<void> { co_return; };
    auto task3 = []() -> tinycoro::Task<void> { co_return; };

    co_await tinycoro::AllOfAwait(scheduler, task1(), task2(), task3());
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
            co_await tinycoro::this_coro::yield_cancellable();
            count++;
        }
        co_return count;
    };

    auto [t1, t2, t3] = co_await tinycoro::AnyOfAwait(scheduler, task1(100ms), task1(2s), task1(3s));

    EXPECT_TRUE(t1.value() > 0);
    EXPECT_FALSE(t2.has_value());
    EXPECT_FALSE(t3.has_value());

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
            co_await tinycoro::this_coro::yield_cancellable();
            count++;
        }
        co_return count;
    };

    auto stopSource = co_await tinycoro::this_coro::stop_source();

    auto results = co_await tinycoro::AnyOfAwait(scheduler, stopSource, task1(100ms), task1(2s), task1(3s));

    EXPECT_TRUE(std::get<0>(results).value() > 0);
    EXPECT_FALSE(std::get<1>(results).has_value());
    EXPECT_FALSE(std::get<2>(results).has_value());

    EXPECT_TRUE(std::chrono::system_clock::now() - now < 500ms);
}

TEST(AnyOfCoAwaitTest2, AnyOfCoAwaitTest2)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto future = scheduler.Enqueue(AnyOfCoAwaitTest2(scheduler));
    EXPECT_NO_THROW(tinycoro::GetAll(future));
}

TEST_F(AllOfAwaiterTest, AllOfAwaiterTest_callOrder)
{
    tinycoro::Scheduler scheduler{1};

    auto task = [](tinycoro::Scheduler& scheduler) -> tinycoro::Task<std::string> {
        uint32_t count{};

        auto Toast = [&count]() -> tinycoro::Task<std::string> {
            EXPECT_EQ(count++, 0); // Need to call first
            co_return "toast";
        };

        auto Coffee = [&count]() -> tinycoro::Task<std::string> {
            EXPECT_EQ(count++, 1); // Need to call second
            co_return "coffee";
        };

        auto Tee = [&count]() -> tinycoro::Task<std::string> {
            EXPECT_EQ(count++, 2); // Need to call third
            co_return "tee";
        };

        // The `AllOfAwait` ensures both `Toast()` and `Coffee()` are executed concurrently.
        auto [toast, coffee, tee] = co_await tinycoro::AllOfAwait(scheduler, Toast(), Coffee(), Tee());
        co_return *toast + " + " + *coffee + " + " + *tee;
    };

    // Start the asynchronous execution of the Breakfast task.
    auto breakfast = tinycoro::AllOf(scheduler, task(scheduler));
    EXPECT_TRUE(breakfast == "toast + coffee + tee");
}

struct AllOfAwaiterDynamicTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(AllOfAwaiterDynamicTest, AllOfAwaiterDynamicTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(AllOfAwaiterDynamicTest, AllOfAwaiterDynamicFuntionalTest_1)
{
    tinycoro::Scheduler scheduler{16};

    const auto size = GetParam();

    std::atomic<size_t> count{};

    auto task = [&count]() -> tinycoro::Task<size_t> { co_return ++count; };

    auto coro = [&]() -> tinycoro::Task<void> {
        std::vector<tinycoro::Task<size_t>> tasks;

        for (size_t i = 0; i < size; ++i)
        {
            tasks.push_back(task());
        }

        auto results = co_await tinycoro::AllOfAwait(scheduler, std::move(tasks));

        EXPECT_EQ(results.size(), count);

        // check for unique values
        std::set<size_t> set;
        for (auto it : results)
        {
            // no lock needed here only one consumer
            auto [_, inserted] = set.insert(*it);
            EXPECT_TRUE(inserted);
        }
    };

    tinycoro::AllOf(coro());
}

TEST_P(AllOfAwaiterDynamicTest, AnyOfAwaitDynamicFuntionalTest_1)
{
    tinycoro::Scheduler scheduler{16};

    const auto size = GetParam();

    std::atomic<size_t> count{};

    auto task = [&count]() -> tinycoro::Task<size_t> { co_return ++count; };

    auto coro = [&]() -> tinycoro::Task<void> {
        std::vector<tinycoro::Task<size_t>> tasks;
        tasks.reserve(size);

        for (size_t i = 0; i < size; ++i)
        {
            tasks.push_back(task());
        }

        auto results = co_await tinycoro::AnyOfAwait(scheduler, std::move(tasks));

        // check for unique values
        std::set<size_t> set;
        for (auto& it : results)
        {
            if (it.has_value())
            {
                // no lock needed here only one consumer
                auto [_, inserted] = set.insert(*it);
                EXPECT_TRUE(inserted);
            }
        }
    };

    tinycoro::AllOf(coro());
}

TEST_P(AllOfAwaiterDynamicTest, AnyOfAwaitDynamicFuntionalTest_2)
{
    // only 1 thread
    tinycoro::Scheduler scheduler{1};

    const auto size = GetParam();

    std::atomic<size_t> count{};

    auto task = [](auto& c) -> tinycoro::Task<size_t> {
        co_await tinycoro::this_coro::yield_cancellable();
        co_return ++c;
    };

    auto coro = [&]() -> tinycoro::Task<void> {
        std::vector<tinycoro::Task<size_t>> tasks;

        for (size_t i = 0; i < size; ++i)
        {
            tasks.push_back(task(count));
        }

        auto results = co_await tinycoro::AnyOfAwait(scheduler, std::move(tasks));

        EXPECT_EQ(count, 1);
        EXPECT_EQ(results.size(), size);

        EXPECT_EQ(count, std::ranges::count_if(results, [](auto& it) { return it.has_value(); }));
    };

    tinycoro::AllOf(coro());
}

TEST(AllOfAwaiterDynamicTest, AnyOfAwaitDynamicFuntionalTest_3)
{
    tinycoro::SoftClock clock;
    // scheduler with only 1 thread
    tinycoro::Scheduler scheduler{1};

    std::atomic<size_t> count{};

    auto coro = [&]() -> tinycoro::Task<void> {
        auto task = [&](auto duration) -> tinycoro::Task<void> {
            co_await tinycoro::SleepForCancellable(clock, duration);
            ++count; // should never reach this code
        };

        std::vector<tinycoro::Task<void>> tasks;

        {
            auto t2 = [](auto& c) -> tinycoro::Task<void> {
                ++c;
                co_return;
            };

            tasks.push_back(t2(count));
        }

        tasks.push_back(task(1000ms));
        tasks.push_back(task(2000ms));

        co_await tinycoro::AnyOfAwait(scheduler, std::move(tasks));

        EXPECT_EQ(count, 1);
    };

    tinycoro::AllOf(coro());
}

TEST(AllOfAwaiterDynamicTest, AnyOfAwaitDynamicFuntionalTest_exception)
{
    tinycoro::SoftClock clock;
    // scheduler with only 1 thread
    tinycoro::Scheduler scheduler{1};

    std::atomic<size_t> count{};

    auto task = [&count, &clock](auto duration) -> tinycoro::Task<void> {
        co_await tinycoro::SleepForCancellable(clock, duration);
        ++count; // should never reach this code
    };

    auto coro = [&]() -> tinycoro::Task<void> {
        std::vector<tinycoro::Task<void>> tasks;

        auto t2 = [&]() -> tinycoro::Task<void> {
            throw std::runtime_error{"exception"};
            co_return;
        };

        tasks.push_back(t2());

        tasks.push_back(task(1000ms));
        // tasks.push_back(task(2000ms));

        EXPECT_THROW(co_await tinycoro::AnyOfAwait(scheduler, std::move(tasks)), std::runtime_error);

        EXPECT_EQ(count, 0);
    };

    tinycoro::AllOf(coro());
}