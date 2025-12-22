#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <set>

#include <tinycoro/tinycoro_all.h>

#include "Allocator.hpp"

struct MutexTest : testing::TestWithParam<size_t>
{
    void SetUp() override
    {
        s_allocator.release();
    }

    static inline tinycoro::test::Allocator<MutexTest,  20000000u> s_allocator;

    template<typename T>
    using AllocatorT = tinycoro::test::AllocatorAdapter<T, decltype(s_allocator)>;
};

INSTANTIATE_TEST_SUITE_P(MutexTest, MutexTest, testing::Values(1, 10, 100, 1000, 10'000, 100'000));

template <typename T, typename U>
class PopAwaiterMock : tinycoro::detail::SingleLinkable<PopAwaiterMock<T, U>>
{
public:
    PopAwaiterMock(auto&, auto) { }
};

TEST(MutexTest, MutexTest_coawaitReturn)
{
    tinycoro::detail::Mutex<PopAwaiterMock> mutex;

    auto awaiter = mutex.operator co_await();

    using expectedAwaiterType = PopAwaiterMock<decltype(mutex), tinycoro::detail::ResumeSignalEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(MutexTest, MutexTest_await_ready) { }

TEST_P(MutexTest, MutexFunctionalTest_1)
{
    tinycoro::Mutex mutex;

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<size_t, MutexTest::AllocatorT> {
        auto lock = co_await mutex;
        co_return ++count;
    };

    std::vector<tinycoro::Task<size_t, MutexTest::AllocatorT>> tasks;

    auto size = GetParam();

    for (size_t i = 0; i < size; ++i)
    {
        tasks.push_back(task());
    }

    auto results = tinycoro::AllOf(scheduler, std::move(tasks));

    // check for unique values
    std::set<size_t> set;
    for (auto it : results)
    {
        auto [_, inserted] = set.insert(*it);
        EXPECT_TRUE(inserted);
    }

    EXPECT_EQ(count, size);
}

struct MutexStressTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(MutexStressTest, MutexStressTest, testing::Values(100, 1'000, 10'000, 100'000));

TEST_P(MutexStressTest, MutexFunctionalStressTest_1)
{
    tinycoro::Mutex mutex;

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    const auto size = GetParam();

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<void> {
        for (size_t i = 0; i < size; ++i)
        {
            auto lock = co_await mutex;
            ++count;
        }
    };

    // starting 8 async tasks at the same time
    tinycoro::AllOf(scheduler, task(), task(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(count, size * 8);
}