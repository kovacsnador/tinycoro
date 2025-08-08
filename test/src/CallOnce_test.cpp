#include <gtest/gtest.h>

#include "tinycoro/CallOnce.hpp"
#include "tinycoro/tinycoro_all.h"

TEST(CallOnceTest, CallOnceTest)
{
    int32_t count{};

    std::atomic_flag flag;
    auto res = tinycoro::detail::CallOnce(flag, std::memory_order::acq_rel, [&]{ ++count; });

    EXPECT_TRUE(res);
    EXPECT_EQ(count, 1);

    auto res2 = tinycoro::detail::CallOnce(flag, std::memory_order::acq_rel, [&]{ ++count; });
    EXPECT_FALSE(res2);
    EXPECT_EQ(count, 1);
}

struct CallOnceTest : testing::TestWithParam<int32_t>
{
};

INSTANTIATE_TEST_SUITE_P(CallOnceTest, CallOnceTest, testing::Values(1, 10, 100, 500));

TEST_P(CallOnceTest, CallOnceTest_threads)
{
    auto count = GetParam();
 
    int32_t desired{};
    std::atomic_flag flag;

    tinycoro::Scheduler scheduler;
    tinycoro::Barrier barrier{8};

    auto task = [&]()->tinycoro::Task<> {
        
        for(int32_t i = 0; i < count;)
        {
            tinycoro::detail::CallOnce(flag, std::memory_order::acq_rel, [&]{ ++desired; });
            co_await barrier.ArriveAndWait();

            EXPECT_EQ(desired, ++i);

            flag.clear();

            co_await barrier.ArriveAndWait();

            EXPECT_FALSE(flag.test());

            co_await barrier.ArriveAndWait();
        }
    };

    tinycoro::AllOf(scheduler, task(), task(), task(), task(), task(), task(), task(), task());
}