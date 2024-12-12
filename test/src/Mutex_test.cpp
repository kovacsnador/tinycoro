#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <set>

#include <tinycoro/tinycoro_all.h>

struct MutexTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(
    MutexTest,
    MutexTest,
    testing::Values(1, 10, 100, 1000, 20'000)
 );
 
template <typename, typename>
class AwaiterMock
{
public:
    AwaiterMock(auto&, auto) { }

    AwaiterMock* next{nullptr};
};

TEST(MutexTest, MutexTest_coawaitReturn)
{
    tinycoro::detail::Mutex<AwaiterMock> mutex;

    auto awaiter = mutex.operator co_await();

    using expectedAwaiterType = AwaiterMock<decltype(mutex), tinycoro::detail::PauseCallbackEvent>;
    EXPECT_TRUE((std::same_as<expectedAwaiterType, decltype(awaiter)>));
}

TEST_P(MutexTest, MutexFunctionalTest_1)
{
    tinycoro::Mutex mutex;

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    size_t count{0};

    auto task = [&]()->tinycoro::Task<size_t> {
        auto lock = co_await mutex;
        co_return ++count;
    };

    std::vector<tinycoro::Task<size_t>> tasks;

    auto size = GetParam();

    for(size_t i = 0; i < size; ++i)
    {
        tasks.push_back(task());
    }
    
    auto results = tinycoro::GetAll(scheduler, tasks);

    // check for unique values
    std::set<size_t> set;
    for (auto it : results)
    {
        auto [_, inserted] = set.insert(it);
        EXPECT_TRUE(inserted);
    }

    EXPECT_EQ(count, size);
}