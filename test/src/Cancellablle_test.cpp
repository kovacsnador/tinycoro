#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

TEST(CancellableTest, CancellableTest)
{
    tinycoro::Scheduler scheduler{2};
    tinycoro::Latch latch{2};

    auto task1 = []()->tinycoro::Task<> { co_return; };

    auto taskToCancel = [&]() -> tinycoro::Task<int32_t> { 
        co_await tinycoro::Cancellable{latch.Wait()};
        co_return 42;
    };

    auto [res1, res2] =  tinycoro::AnyOf(scheduler, task1(), taskToCancel());

    EXPECT_FALSE(res2.has_value());
}