#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <coroutine>
#include <ranges>
#include <algorithm>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/tinycoro_all.h>

template <typename T>
struct SemaphoreMock
{
    MOCK_METHOD(void, Release, ());
    MOCK_METHOD(bool, _TryAcquire, (void*, tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>>));
    MOCK_METHOD(bool, TryAcquire, ());
};

struct SemaphoreAwaiterTest : public testing::Test
{
    using value_type = int32_t;

    SemaphoreAwaiterTest()
    : awaiter{mock, tinycoro::detail::ResumeSignalEvent{}}
    {
    }

    void SetUp() override
    {
        hdl.promise().CreateSharedState();
        hdl.promise().SharedState()->ResetCallback(tinycoro::ResumeCallback_t{});
    }

    SemaphoreMock<value_type> mock;

    tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<value_type>> hdl;

    tinycoro::detail::SemaphoreAwaiter<decltype(mock), tinycoro::detail::ResumeSignalEvent> awaiter;
};

TEST_F(SemaphoreAwaiterTest, SemaphoreAwaiterTest_AcquireSucceded)
{
    EXPECT_CALL(mock, TryAcquire()).Times(1).WillOnce(testing::Return(false));

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(awaiter.next, nullptr);

    EXPECT_CALL(mock, _TryAcquire(&awaiter, hdl)).Times(1).WillOnce(testing::Return(true));
    EXPECT_FALSE(awaiter.await_suspend(hdl));

    EXPECT_CALL(mock, Release()).Times(1);
    {
        auto lock = awaiter.await_resume();
    }
}

TEST_F(SemaphoreAwaiterTest, SemaphoreAwaiterTest_AcquireFalied)
{
    EXPECT_CALL(mock, TryAcquire()).Times(1).WillOnce(testing::Return(false));

    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(awaiter.next, nullptr);

    EXPECT_CALL(mock, _TryAcquire(&awaiter, hdl)).Times(1).WillOnce(testing::Return(false));
    EXPECT_TRUE(awaiter.await_suspend(hdl));

    EXPECT_CALL(mock, Release()).Times(1);
    {
        auto lock = awaiter.await_resume();
    }
}

template <typename SemaphoreT, typename EventT>
class PopAwaiterMock : public tinycoro::detail::SingleLinkable<PopAwaiterMock<SemaphoreT, EventT>>
{
public:
    PopAwaiterMock(SemaphoreT& s, EventT e)
    : semaphore{s}
    , event{std::move(e)}
    {
    }

    MOCK_METHOD(void, Notify, (), (const));
    MOCK_METHOD(void, PutOnPause, (tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<int32_t>>));

    auto TestTryAcquire(auto parentCoro) { return semaphore._TryAcquire(this, parentCoro); }

    auto TestRelease() { return semaphore.Release(); }

    SemaphoreT&  semaphore;
    EventT event;
};

template<typename T>
struct SemaphoreTest : testing::Test
{
    using value_type = T;

    static constexpr auto count = value_type::value;

    using semaphore_type  = tinycoro::detail::Semaphore<count, PopAwaiterMock, tinycoro::detail::LinkedPtrQueue>;
    using corohandle_type = tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<int32_t>>;

    void SetUp() override
    {
        hdl.promise().CreateSharedState();
        hdl.promise().SharedState()->ResetCallback(tinycoro::ResumeCallback_t{});
    }

    corohandle_type hdl;
};

template<size_t V>
struct SizeValue
{
    static constexpr size_t value = V;
};

using SemaphoreTestTypes = testing::Types<SizeValue<1>, SizeValue<5>, SizeValue<10>, SizeValue<100>, SizeValue<1000>, SizeValue<10000>>; 

TYPED_TEST_SUITE(SemaphoreTest, SemaphoreTestTypes);

using SemaphoreFixture = SemaphoreTest<SizeValue<1>>;

TEST_F(SemaphoreFixture, SemaphoreTest_counter_1)
{
    semaphore_type semaphore;

    auto mock = semaphore.operator co_await();

    EXPECT_CALL(mock, PutOnPause(hdl)).Times(2);
    EXPECT_TRUE(mock.TestTryAcquire(hdl));
    EXPECT_FALSE(mock.TestTryAcquire(hdl));

    EXPECT_CALL(mock, Notify()).Times(2);
    mock.TestRelease();

    EXPECT_FALSE(mock.TestTryAcquire(hdl));
    mock.TestRelease();
    mock.TestRelease();

    EXPECT_TRUE(mock.TestTryAcquire(hdl));
}

TYPED_TEST(SemaphoreTest, SemaphoreTest_counter_param)
{
    using T = typename TestFixture::value_type;

    const auto count = T::value;

    typename TestFixture::semaphore_type semaphore;

    auto mock = semaphore.operator co_await();

    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        EXPECT_TRUE(mock.TestTryAcquire(this->hdl));
    }

    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        mock.TestRelease();
    }

    for ([[maybe_unused]] auto it : std::views::iota(0u, count))
    {
        EXPECT_TRUE(mock.TestTryAcquire(this->hdl));
    }

    EXPECT_CALL(mock, PutOnPause).Times(1);
    EXPECT_CALL(mock, Notify()).Times(1);

    EXPECT_FALSE(mock.TestTryAcquire(this->hdl));
    mock.TestRelease();
    mock.TestRelease();

    EXPECT_TRUE(mock.TestTryAcquire(this->hdl));
}

TYPED_TEST(SemaphoreTest, SemaphoreTest_multi_release)
{
    using T = typename TestFixture::value_type;

    constexpr auto count = T::value;

    // blocked semaphore
    tinycoro::Semaphore<count> semaphore{0};

    tinycoro::Scheduler scheduler;

    std::atomic<size_t> counter{};

    auto starter = [&]() -> tinycoro::Task<> { 
        // release all the semaphores
        semaphore.Release(count);
        co_return;
    };

    auto worker = [&]() -> tinycoro::Task<> { 
        // auto guard = co_await semaphore;
        // guard.release();    // no auto release
        // or
        tinycoro::IgnoreGuard(co_await semaphore);

        counter.fetch_add(1, std::memory_order::relaxed);

        // semaphore is not released here...
    };

    std::vector<tinycoro::Task<>> tasks;
    tasks.reserve(count + 1);
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(worker());
    }
    tasks.emplace_back(starter());

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, counter);
}

TEST(SemaphoreTest, SemaphoreTest_closedAtBegin)
{
    tinycoro::Scheduler    scheduler;
    tinycoro::Semaphore<2> semaphore{0};

    int32_t count{0};

    auto releaser = [&semaphore, &count]() -> tinycoro::Task<int32_t> {
        auto ret = count;
        semaphore.Release(2);
        co_return ret;
    };

    auto acquires = [&semaphore, &count](int32_t v) -> tinycoro::Task<int32_t> {
        auto guard = co_await semaphore;
        co_return count + v;
    };

    auto [c1, c2, c3] = tinycoro::AllOf(scheduler, releaser(), acquires(1), acquires(2));

    EXPECT_EQ(c1, 0);
    EXPECT_EQ(c2, 1);
    EXPECT_EQ(c3, 2);
}

TEST(SemaphoreTest, SemaphoreTest_closedAtBegin_blocking_acquire)
{
    tinycoro::Scheduler    scheduler;
    tinycoro::Semaphore<2> semaphore{0};

    int32_t count{0};

    auto releaser = [&semaphore, &count]() -> tinycoro::Task<int32_t> {
        auto ret = count;
        semaphore.Release(2);
        co_return ret;
    };

    auto acquires = [&semaphore, &count](int32_t v) -> tinycoro::Task<int32_t> {
        semaphore.Acquire();    // blocking wait
        co_return count + v;
    };

    auto [c1, c2, c3] = tinycoro::AllOf(scheduler, releaser(), acquires(1), acquires(2));

    EXPECT_EQ(c1, 0);
    EXPECT_EQ(c2, 1);
    EXPECT_EQ(c3, 2);
}

TEST(SemaphoreTest, SemaphoreTest_TryAcquire_failed)
{
    tinycoro::Semaphore<2> semaphore{0};

    for (size_t i = 0; i < 1000; ++i)
        EXPECT_FALSE(semaphore.TryAcquire());
}

TEST(SemaphoreTest, SemaphoreTest_TryAcquire_with_release)
{
    tinycoro::Semaphore<100> semaphore;

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(semaphore.TryAcquire());

    for (size_t i = 0; i < 100; ++i)
        EXPECT_FALSE(semaphore.TryAcquire());

    semaphore.Release(semaphore.Max());

    for (size_t i = 0; i < 100; ++i)
        EXPECT_TRUE(semaphore.TryAcquire());
}

struct SemaphoreFunctionalTest : public testing::TestWithParam<int32_t>
{
    tinycoro::CustomScheduler<2> scheduler;
};

INSTANTIATE_TEST_SUITE_P(SemaphoreFunctionalTest,
                         SemaphoreFunctionalTest,
                         testing::Values(1, 5, 10, 100, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 10000));

TEST_F(SemaphoreFunctionalTest, SemaphoreFunctionalTest_exampleTest)
{
    tinycoro::Semaphore<1> semaphore{1};

    int32_t count{0};

    auto task = [&semaphore, &count]() -> tinycoro::Task<int32_t> {
        auto lock = co_await semaphore;
        co_return ++count;
    };

    auto [c1, c2, c3] = tinycoro::AllOf(scheduler, task(), task(), task());

    EXPECT_TRUE(c1 != c2 && c2 != c3 && c3 != c1);
    EXPECT_EQ(count, 3);
}

TEST_P(SemaphoreFunctionalTest, SemaphoreFunctionalTest_counter)
{
    const auto param = GetParam();

    tinycoro::BinarySemaphore semaphore{1};

    int32_t count{0};

    auto task = [&semaphore, &count]() -> tinycoro::Task<void> {
        auto lock1 = co_await semaphore;
        count++;
    };

    std::vector<tinycoro::Task<void>> tasks;
    for ([[maybe_unused]] int32_t i = 0; i < param; ++i)
    {
        tasks.emplace_back(task());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
    EXPECT_EQ(count, param);
}

TEST_P(SemaphoreFunctionalTest, SemaphoreFunctionalTest_counter_double)
{
    const auto param = GetParam();

    tinycoro::BinarySemaphore semaphore{1};

    int32_t count{0};

    auto task = [&semaphore, &count]() -> tinycoro::Task<void> {
        {
            auto lock = co_await semaphore;
            count++;
        }
        {
            auto lock = co_await semaphore;
            count++;
        }
    };

    std::vector<tinycoro::Task<void>> tasks;
    for ([[maybe_unused]] int32_t i = 0; i < param; ++i)
    {
        tasks.emplace_back(task());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
    EXPECT_EQ(count, param * 2);
}

TEST_P(SemaphoreFunctionalTest, SemaphoreFunctionalTest_counter_max)
{
    const auto param = GetParam();

    tinycoro::Semaphore<4> semaphore;

    std::atomic_int32_t currentAllowed{0};
    int32_t             max{0};

    auto task = [&semaphore, &max, &currentAllowed]() -> tinycoro::Task<void> {
        {
            auto lock = co_await semaphore;

            currentAllowed++;
            max = std::max(max, currentAllowed.load());
            currentAllowed--;
        }
        {
            auto lock = co_await semaphore;

            currentAllowed++;
            max = std::max(max, currentAllowed.load());
            currentAllowed--;
        }
    };

    std::vector<tinycoro::Task<void>> tasks;
    for ([[maybe_unused]] int32_t i = 0; i < param; ++i)
    {
        tasks.emplace_back(task());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
    EXPECT_TRUE(max <= 4);
}

struct SemaphoreStressTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SemaphoreStressTest, SemaphoreStressTest, testing::Values(100, 1'000, 10'000));

TEST_P(SemaphoreStressTest, SemaphoreStressTest_1)
{
    tinycoro::BinarySemaphore semaphore;

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto size = GetParam();

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < size; ++i)
        {
            auto lock = co_await semaphore;
            ++count;
        }
    };

    // starting 8 async tasks at the same time
    tinycoro::AllOf(scheduler, task(), task(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(count, size * 8);
}

TEST_P(SemaphoreStressTest, SemaphoreStressTest_blocking)
{
    const size_t taskCount = 8;

    tinycoro::BinarySemaphore semaphore;

    tinycoro::Scheduler scheduler{taskCount};

    const auto size = GetParam();

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < size; ++i)
        {
            semaphore.Acquire();
            ++count;
            semaphore.Release();
        }

        co_return;
    };

    // starting 8 async tasks at the same time
    tinycoro::AllOf(scheduler, task(), task(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(count, size * taskCount);
}

TEST_P(SemaphoreStressTest, SemaphoreStressTest_blocking_try_acquire)
{
    const size_t taskCount = 8;

    tinycoro::BinarySemaphore semaphore;

    tinycoro::Scheduler scheduler{taskCount};

    const auto size = GetParam();

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < size; ++i)
        {
            // spin acquire
            while (semaphore.TryAcquire() == false) ;
            ++count;
            semaphore.Release();
        }

        co_return;
    };

    // starting 8 async tasks at the same time
    tinycoro::AllOf(scheduler, task(), task(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(count, size * taskCount);
}

struct SemaphoreTimeoutTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SemaphoreTimeoutTest, SemaphoreTimeoutTest, testing::Values(1, 10, 100, 1000, 10000));

TEST_P(SemaphoreTimeoutTest, SemaphoreTimeoutTest_timeout_all)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    // sema is locked
    tinycoro::BinarySemaphore sema{0};

    std::atomic<size_t> timeoutCount{};

    auto func = [&]() -> tinycoro::TaskNIC<> { 
        auto guard = co_await tinycoro::TimeoutAwait{clock, sema.Wait(), 20ms};
        if (guard.has_value() == false)
            timeoutCount++;
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(func());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(timeoutCount, count);
}

TEST_P(SemaphoreTimeoutTest, SemaphoreTimeoutTest_timeout_race)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    // sema is locked
    tinycoro::Semaphore<1> sema;

    size_t succeessCount{};

    auto func = [&]() -> tinycoro::TaskNIC<> {
        auto guard = co_await tinycoro::TimeoutAwait{clock, sema.Wait(), 10ms};
        if (guard.has_value())
            succeessCount++;
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(func());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    // intended to check woth sanitizers...
    EXPECT_TRUE(succeessCount > 0);
}

TEST_P(SemaphoreTimeoutTest, SemaphoreTimeoutTest_cancel_all_after)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    // sema is locked
    tinycoro::BinarySemaphore sema{0};

    std::atomic<size_t> cancelCount{};

    auto func = [&]() -> tinycoro::TaskNIC<> {
        auto guard = co_await tinycoro::Cancellable{sema.Wait()};
        cancelCount++;
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count + 1);

    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(func());
    }

    tasks.emplace_back([]() -> tinycoro::TaskNIC<> { co_return; }());

    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(cancelCount, 0);
}

TEST_P(SemaphoreTimeoutTest, SemaphoreTimeoutTest_cancel_all_before)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    // sema is locked
    tinycoro::BinarySemaphore sema{0};

    std::atomic<size_t> cancelCount{};

    auto func = [&]() -> tinycoro::TaskNIC<> {
        auto guard = co_await tinycoro::Cancellable{sema.Wait()};
        cancelCount++;
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count + 1);

    tasks.emplace_back([]() -> tinycoro::TaskNIC<> { co_return; }());
    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(func());
    }

    tinycoro::AnyOf(scheduler, std::move(tasks));

    EXPECT_EQ(cancelCount, 0);
}

TEST_P(SemaphoreTimeoutTest, SemaphoreTimeoutTest_cancel_all_race)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::SoftClock clock;

    // sema is locked
    tinycoro::Semaphore<1> sema;

    size_t cancelCount{};

    auto func = [&]() -> tinycoro::TaskNIC<> {
        auto guard = co_await tinycoro::Cancellable{sema.Wait()};
        cancelCount++;
    };

    std::vector<tinycoro::TaskNIC<>> tasks;
    tasks.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        tasks.emplace_back(func());
    }

    tinycoro::AnyOf(scheduler, std::move(tasks));

    // intended to check with sanitizers...
    EXPECT_TRUE(cancelCount > 0);
}