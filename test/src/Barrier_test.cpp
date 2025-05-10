#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>

#include <tinycoro/tinycoro_all.h>

#include "mock/CoroutineHandleMock.h"

struct BarrierTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BarrierTest,
                         BarrierTest,
                         testing::Values(1,
                                         5,
                                         10,
                                         100,100,100,100,100,100,
                                         100,100,100,100,100,100,
                                         10000));

TEST_P(BarrierTest, BarrierTest_arrive)
{
    const auto count = GetParam();

    bool complete{false};
    auto complition = [&] { complete = true; };

    tinycoro::Barrier barrier{count, complition};

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        auto result = barrier.Arrive();
        EXPECT_EQ(result, complete);
    }

    EXPECT_TRUE(complete);
    complete = false;

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        auto result = barrier.Arrive();
        EXPECT_EQ(result, complete);
    }

    EXPECT_TRUE(complete);
}

TEST_P(BarrierTest, BarrierTest_arriveAndDrop)
{
    const auto count = GetParam();

    bool complete{false};
    auto complition = [&] { complete = true; };

    tinycoro::Barrier barrier{count, complition};

    for (size_t i = 0; i < count; ++i)
    {
        EXPECT_FALSE(complete);
        barrier.ArriveAndDrop();
    }

    EXPECT_TRUE(complete);
    complete = false;

    // total count should be 0 here, so no suspend any more
    EXPECT_TRUE(barrier.Arrive());
    EXPECT_TRUE(complete);
}

TEST(BarrierTest, BarrierTest_constructor)
{
    EXPECT_NO_THROW(tinycoro::Barrier{10});
    EXPECT_THROW(tinycoro::Barrier{0}, tinycoro::BarrierException);

    EXPECT_NO_THROW((tinycoro::Barrier{10, [] { }}));
    EXPECT_THROW((tinycoro::Barrier{0, [] { }}), tinycoro::BarrierException);
}

using DecrementData = std::tuple<int32_t, int32_t, int32_t>;

struct DecrementTest : testing::TestWithParam<DecrementData>
{
};

INSTANTIATE_TEST_SUITE_P(DecrementTest,
                         DecrementTest,
                         testing::Values(DecrementData{3, 10, 2},
                                         DecrementData{2, 10, 1},
                                         DecrementData{1, 10, 10},
                                         DecrementData{0, 10, 10},
                                         DecrementData{0, 0, 0},
                                         DecrementData{10, 0, 9},
                                         DecrementData{1, 0, 0}));

TEST_P(DecrementTest, BarrierTest_Decrement)
{
    auto [val, reset, expected] = GetParam();
    EXPECT_EQ(tinycoro::detail::local::Decrement(val, reset), expected);
}

template <typename BarrierT, typename EventT>
class BarrierAwaiterMock : public tinycoro::detail::BarrierAwaiter<BarrierAwaiterMock<BarrierT, EventT>, EventT>
{
    using BaseT = tinycoro::detail::BarrierAwaiter<BarrierAwaiterMock<BarrierT, EventT>, EventT>;

public:
    BarrierAwaiterMock(BarrierT& b, EventT e, auto p)
    : BaseT{*this, e, p}
    , barrier{b}
    {
    }

    bool Add(auto, auto policy) { return barrier.Add(this, policy); }

    MOCK_METHOD(void, Notify, ());

    BarrierAwaiterMock* next{nullptr};

    BarrierT& barrier;
};

TEST(BarrierTest, BarrierTest_coawaitReturn)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, BarrierAwaiterMock> barrier{10};

    auto awaiter = barrier.operator co_await();

    using expectedAwaiterType = BarrierAwaiterMock<decltype(barrier), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(BarrierTest, BarrierTest_arriveAndWait)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, BarrierAwaiterMock> barrier{2};

    auto awaiter = barrier.ArriveAndWait();

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });

    EXPECT_TRUE(awaiter.await_suspend(hdl));

    EXPECT_CALL(awaiter, Notify()).Times(1);
    barrier.Arrive();
}

TEST(BarrierTest, BarrierTest_await_suspend)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, BarrierAwaiterMock> barrier{2};

    barrier.Arrive();

    auto awaiter = barrier.ArriveAndWait();

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });

    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

TEST(BarrierTest, BarrierTest_await_suspend_dropWait)
{
    tinycoro::Barrier<tinycoro::detail::NoopComplitionCallback, BarrierAwaiterMock> barrier{2};

    barrier.Arrive();

    auto awaiter = barrier.ArriveDropAndWait();

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });

    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

struct BarrierComplitionMock
{
    struct ImplMock
    {
        MOCK_METHOD(void, Invoke, (), (const));
    };

    BarrierComplitionMock() { mock = std::make_shared<ImplMock>(); }

    void operator()() const { mock->Invoke(); }

    std::shared_ptr<ImplMock> mock;
};

TEST(BarrierTest, BarrierTest_safeInvoke)
{
    // callable mock
    BarrierComplitionMock mock;

    EXPECT_CALL(*mock.mock, Invoke()).Times(2);

    EXPECT_TRUE(tinycoro::detail::local::SafeRegularInvoke(mock));
    EXPECT_TRUE(tinycoro::detail::local::SafeRegularInvoke(mock));

    struct NotCallable
    {
    };

    EXPECT_FALSE(tinycoro::detail::local::SafeRegularInvoke(NotCallable{}));
    EXPECT_FALSE(tinycoro::detail::local::SafeRegularInvoke(int32_t{}));
}

TEST(BarrierTest, BarrierTest_arriveAndWait_after)
{
    BarrierComplitionMock complitionMock;

    tinycoro::Barrier barrier{2, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(1);

    barrier.Arrive();
    auto awaiter = barrier.ArriveAndWait();

    EXPECT_FALSE(awaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

TEST_P(BarrierTest, BarrierTest_complitionCallback_arrive)
{
    const size_t count = GetParam();

    BarrierComplitionMock complitionMock;

    tinycoro::Barrier barrier{count, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(2);

    for (size_t i = 0; i < count; ++i)
    {
        barrier.Arrive();
        barrier.Arrive();
    }
}

TEST(BarrierTest, BarrierTest_await_ready_and_suspend)
{
    tinycoro::Barrier barrier{2};

    auto awaiter = barrier.Wait();

    EXPECT_FALSE(awaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_TRUE(awaiter.await_suspend(hdl));
}

TEST(BarrierTest, BarrierTest_await_ready_and_suspend_ready)
{
    tinycoro::Barrier barrier{2};

    barrier.Arrive();

    auto awaiter = barrier.ArriveAndWait();

    EXPECT_FALSE(awaiter.await_ready());

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });
    EXPECT_FALSE(awaiter.await_suspend(hdl));
}

TEST(BarrierTest, BarrierTest_notifyAndComplition)
{
    BarrierComplitionMock complitionMock;

    tinycoro::Barrier<BarrierComplitionMock, BarrierAwaiterMock> barrier{3, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(1);

    auto awaiter = barrier.Wait();

    auto hdl = tinycoro::test::MakeCoroutineHdl([] { });

    EXPECT_TRUE(awaiter.await_suspend(hdl));

    EXPECT_CALL(awaiter, Notify()).Times(1);

    barrier.Arrive();
    barrier.Arrive();
    barrier.Arrive();
}

TEST_P(BarrierTest, BarrierFunctionalTest_1)
{
    const size_t count = GetParam();

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    BarrierComplitionMock complitionMock;
    tinycoro::Barrier     barrier{count, complitionMock};

    EXPECT_CALL(*complitionMock.mock, Invoke()).Times(3);

    std::atomic<size_t> number{};

    auto task = [&]() -> tinycoro::Task<void> {
        number++;
        co_await barrier.ArriveAndWait();
        EXPECT_EQ(count, number);

        co_await barrier.ArriveAndWait();
        number--;

        co_await barrier.ArriveAndWait();
        EXPECT_EQ(0, number);
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(task());
    }

    tinycoro::GetAll(scheduler, std::move(tasks));

    EXPECT_EQ(number, 0);
}

TEST(BarrierTest, BarrierFunctionalTest_2)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    std::vector<std::string> workers = {"Anil", "Busara", "Carl"};

    size_t completionCount{};

    auto on_completion = [&]() noexcept {
        // locking not needed here
        if (completionCount == 0)
        {
            for (const auto& it : workers)
            {
                EXPECT_NE(it.find("worked"), std::string::npos);
            }
        }
        else if (completionCount == 1)
        {
            for (const auto& it : workers)
            {
                EXPECT_NE(it.find("cleand"), std::string::npos);
            }
        }

        ++completionCount;
    };

    tinycoro::Barrier sync_point(std::ssize(workers), on_completion);

    auto work = [&sync_point](std::string& name) -> tinycoro::Task<std::string> {
        name += " worked";
        co_await sync_point.ArriveAndWait();

        name += " cleand";
        co_await sync_point.ArriveAndWait();

        co_return name;
    };

    auto [anil, busara, carl] = tinycoro::GetAll(scheduler, work(workers[0]), work(workers[1]), work(workers[2]));

    EXPECT_EQ(anil, workers[0]);
    EXPECT_EQ(busara, workers[1]);
    EXPECT_EQ(carl, workers[2]);

    EXPECT_EQ(completionCount, 2);
}

TEST(BarrierTest, BarrierFewerTasksThanCount)
{
    // Set up a scheduler for the tasks
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    // Barrier that requires 3 arrivals to complete
    constexpr size_t barrier_count    = 3;
    size_t           completion_count = 0;

    // Completion function for the barrier
    auto on_completion = [&]() noexcept { ++completion_count; };

    // Create a barrier with 4 as the count
    tinycoro::Barrier barrier{barrier_count, on_completion};
    tinycoro::Barrier control{3};

    // Atomic variable to track state between tasks
    std::atomic<size_t> number = 0;

    // Define a task that uses the barrier
    auto task = [&]() -> tinycoro::Task<void> {
        number++;
        control.Arrive();
        co_await barrier.ArriveAndWait(); // Task will wait here as the barrier needs 3 arrivals

        number += 100;
        control.Arrive();
    };

    // Define a task that uses the barrier
    auto controlTask = [&]() -> tinycoro::Task<void> {
        // wait for controll barrier
        co_await control.ArriveAndWait();

        // Validate expectations
        EXPECT_EQ(number, 2); // Number should remain 2 as tasks can't proceed past the barrier
        EXPECT_EQ(completion_count, 0); // Completion function should not be called as the barrier was never satisfied

        // decrement barrier
        barrier.ArriveAndDrop();

        // wait to
        co_await control.ArriveAndWait();

        // Validate expectations
        EXPECT_EQ(number, 202);
        EXPECT_EQ(completion_count, 1);
    };

    // Run all tasks
    tinycoro::GetAll(scheduler, task(), task(), controlTask());

    EXPECT_EQ(number, 202);
    EXPECT_EQ(completion_count, 1);
}

TEST(BarrierTest, BarrierFewerTasksThanCount_withControlComplitionCallback)
{
    // Set up a scheduler for the tasks
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    // Barrier that requires 3 arrivals to complete
    constexpr size_t barrier_count    = 3;
    size_t           completion_count = 0;

    // Completion function for the barrier
    auto on_completion = [&]() noexcept { ++completion_count; };

    // Create a barrier with 4 as the count
    tinycoro::Barrier barrier{barrier_count, on_completion};

    // Atomic variable to track state between tasks
    std::atomic<size_t> number = 0;

    auto controllComplition = [&]() noexcept {
        EXPECT_EQ((completion_count * 200 + 2), number);

        // drop the 3. count
        barrier.ArriveAndDrop();
    };

    tinycoro::Barrier control{2, controllComplition};

    // Define a task that uses the barrier
    auto task = [&]() -> tinycoro::Task<void> {
        number++;
        control.Arrive();
        co_await barrier.ArriveAndWait(); // Task will wait here as the barrier needs 4 arrivals
        // This line should not be executed as there are fewer tasks than needed
        number += 100;
        control.Arrive();
    };

    // Run all tasks
    tinycoro::GetAll(scheduler, task(), task());

    EXPECT_EQ(number, 202);
    EXPECT_EQ(completion_count, 1);
}

TEST_P(BarrierTest, BarrierTest_functionalTest_3)
{
    auto count = GetParam();

    count = std::max(count, size_t{2});

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    std::atomic<size_t> controlCount{0};

    size_t c{1};

    auto onComplition = [&] {
        EXPECT_EQ(controlCount, count * c);
        c++;
    };

    tinycoro::Barrier barrier{count, onComplition};

    auto task = [&]() -> tinycoro::Task<void> {
        controlCount++;
        co_await barrier.ArriveAndWait();
        controlCount++;
        co_await barrier.ArriveAndWait();
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(task());
    }

    tinycoro::GetAll(scheduler, std::move(tasks));
}

TEST(BarrierTest, BarrierTest_functionalTest_cancel_scheduler)
{
    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    tinycoro::Barrier barrier{10};

    auto task = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::Cancellable(barrier.Wait());
        co_return 42;
    };

    auto [r1, r2, r3, r4, r5, r6] = tinycoro::AnyOf(scheduler, task(), task(), task(), task(), task(), tinycoro::SleepFor(clock, 100ms));

    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());
    EXPECT_TRUE(r6.has_value());
}

TEST_P(BarrierTest, BarrierTest_cancel_multi)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    tinycoro::Barrier barrier{count * 3};

    auto task1 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.Wait()); };
    auto task2 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.ArriveAndWait()); };
    auto task3 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.ArriveDropAndWait()); };

    std::vector<tinycoro::Task<void>> tasks;
    tasks.reserve((count * 3) + 1);
    tasks.emplace_back(tinycoro::SleepFor(clock, 100ms));
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(task1());
        tasks.emplace_back(task2());
        tasks.emplace_back(task3());
    }

    EXPECT_NO_THROW(tinycoro::AnyOf(scheduler, std::move(tasks)));
}

TEST(BarrierTest, BarrierTest_preset_stopSource)
{
    tinycoro::Scheduler scheduler;
    tinycoro::Barrier barrier{1};

    std::stop_source stopSource;

    auto task1 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.Wait()); };
    auto task2 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.ArriveAndWait()); };

    stopSource.request_stop();
    tinycoro::AnyOfWithStopSource(scheduler, stopSource, task2(), task1());
}

TEST(BarrierTest, BarrierTest_preset_stopSource_inline)
{
    tinycoro::Scheduler scheduler;
    tinycoro::Barrier barrier{1};

    std::stop_source stopSource;

    auto task1 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.Wait()); };
    auto task2 = [&]() -> tinycoro::Task<void> { co_await tinycoro::Cancellable(barrier.ArriveAndWait()); };

    stopSource.request_stop();
    tinycoro::AnyOfWithStopSourceInline(stopSource, task2(), task1());
}

TEST(BarrierTest, BarrierTest_functionalTest_cancel_inline)
{
    tinycoro::SoftClock clock;

    tinycoro::Barrier barrier{10};

    auto task = [&]() -> tinycoro::Task<int32_t> {
        co_await tinycoro::Cancellable(barrier.Wait());
        co_return 42;
    };

    auto [r1, r2, r3, r4, r5, r6] = tinycoro::AnyOfInline(task(), task(), task(), task(), task(), tinycoro::SleepFor(clock, 100ms));

    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());
    EXPECT_TRUE(r6.has_value());
}

struct BarrierFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BarrierFunctionalTest, BarrierFunctionalTest, testing::Values(2, 5, 10, 100, 200, 300));

TEST_P(BarrierFunctionalTest, BarrierTest_functionalTest_4)
{
    auto count = GetParam();

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    size_t controlCount{0};

    size_t c{1};

    auto onComplition = [&] {
        EXPECT_EQ(controlCount, c);
        c++;
    };

    tinycoro::Barrier barrier{count, onComplition};

    auto arrivel = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            co_await barrier.ArriveAndWait();
        }
    };

    auto worker = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < count; ++i)
        {
            controlCount++;
            co_await barrier.ArriveAndWait();
        }
    };

    std::vector<tinycoro::Task<void>> tasks;
    for (size_t i = 0; i < count - 1; ++i)
    {
        tasks.push_back(arrivel());
    }
    tasks.push_back(worker());

    tinycoro::GetAll(scheduler, std::move(tasks));
}

TEST(BarrierTest, BarrierTest_completionException)
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto onComplition = [] { throw std::runtime_error{"Test Error"}; };

    size_t fullyCompleted{0};

    tinycoro::Barrier barrier{2, onComplition};

    auto task = [&]() -> tinycoro::Task<void> {
        co_await barrier.ArriveAndWait();
        fullyCompleted++;
    };

    EXPECT_THROW(tinycoro::GetAll(scheduler, task(), task()), std::runtime_error);
    EXPECT_EQ(fullyCompleted, 1);
}