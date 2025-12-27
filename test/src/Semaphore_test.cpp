#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <coroutine>
#include <ranges>
#include <algorithm>

#include "mock/CoroutineHandleMock.h"

#include <tinycoro/Semaphore.hpp>
#include <tinycoro/Promise.hpp>
#include <tinycoro/Scheduler.hpp>
#include <tinycoro/Task.hpp>
#include <tinycoro/Wait.hpp>

template <typename T>
struct SemaphoreMock
{
    MOCK_METHOD(void, Release, ());
    MOCK_METHOD(bool, TryAcquire, (void*, tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>>));
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
        hdl.promise().pauseHandler.emplace([](auto) { /* resumer callback */ });
    }

    SemaphoreMock<value_type>                                          mock;
    tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<value_type>> hdl;

    tinycoro::detail::SemaphoreAwaiter<decltype(mock), tinycoro::detail::ResumeSignalEvent> awaiter;
};

TEST_F(SemaphoreAwaiterTest, SemaphoreAwaiterTest_AcquireSucceded)
{
    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(awaiter.next, nullptr);

    EXPECT_CALL(mock, TryAcquire(&awaiter, hdl)).Times(1).WillOnce(testing::Return(true));
    EXPECT_FALSE(awaiter.await_suspend(hdl));

    EXPECT_CALL(mock, Release()).Times(1);
    {
        auto lock = awaiter.await_resume();
    }
}

TEST_F(SemaphoreAwaiterTest, SemaphoreAwaiterTest_AcquireFalied)
{
    EXPECT_FALSE(awaiter.await_ready());
    EXPECT_EQ(awaiter.next, nullptr);

    EXPECT_CALL(mock, TryAcquire(&awaiter, hdl)).Times(1).WillOnce(testing::Return(false));
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

    auto TestTryAcquire(auto parentCoro) { return semaphore.TryAcquire(this, parentCoro); }

    auto TestRelease() { return semaphore.Release(); }

    SemaphoreT&  semaphore;
    EventT event;
};

struct EventMock
{
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
        hdl.promise().pauseHandler.emplace([](auto) { /* resumer callback */ });
    }

    corohandle_type hdl;
};

template<size_t V>
struct SizeValue
{
    static constexpr size_t value = V;
};

using SemaphoreTestTypes = testing::Types<SizeValue<1>, SizeValue<5>, SizeValue<10>, SizeValue<100>, SizeValue<1000>>; 

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

struct SemaphoreFunctionalTest : public testing::TestWithParam<int32_t>
{
    //tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    tinycoro::CustomScheduler<2> scheduler{2};
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