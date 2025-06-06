#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <set>

#include <tinycoro/tinycoro_all.h>

struct MutexTest : testing::TestWithParam<size_t>
{
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

    using expectedAwaiterType = PopAwaiterMock<decltype(mutex), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST(MutexTest, MutexTest_await_ready) { }

TEST_P(MutexTest, MutexFunctionalTest_1)
{
    tinycoro::Mutex mutex;

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    size_t count{0};

    auto task = [&]() -> tinycoro::Task<size_t> {
        auto lock = co_await mutex;
        co_return ++count;
    };

    std::vector<tinycoro::Task<size_t>> tasks;

    auto size = GetParam();

    for (size_t i = 0; i < size; ++i)
    {
        tasks.push_back(task());
    }

    auto results = tinycoro::GetAll(scheduler, std::move(tasks));

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
    tinycoro::GetAll(scheduler, task(), task(), task(), task(), task(), task(), task(), task());

    EXPECT_EQ(count, size * 8);
}