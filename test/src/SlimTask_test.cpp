#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

TEST(SlimTaskTest, SlimTaskTest)
{
    tinycoro::Scheduler scheduler;

    auto slimTask1 = []() -> tinycoro::SlimTask<int32_t> {
        auto slimTask2 = []() -> tinycoro::SlimTask<int32_t> { co_return 41; };

        auto val = co_await slimTask2();
        co_return val += 1;
    };

    auto task = [&]() -> tinycoro::Task<void> {
        auto val = co_await slimTask1();
        EXPECT_EQ(val, 42);
    };

    tinycoro::AllOf(scheduler, task());
}

TEST(SlimTaskTest, SlimTaskTest_run_inline)
{
    auto slimTask1 = []() -> tinycoro::SlimTask<int32_t> {
        auto slimTask2 = []() -> tinycoro::SlimTask<int32_t> { co_return 41; };

        auto val = co_await slimTask2();
        co_return val += 1;
    };

    auto val = tinycoro::AllOf(slimTask1());

    EXPECT_EQ(val, 42);
}

TEST(SlimTaskTest, SlimTaskTest_nested_resume)
{
    auto counter = [](int32_t val) -> tinycoro::SlimTask<int32_t> {
        co_return val + 1;
    };

    auto task1 = [&](int32_t val) -> tinycoro::SlimTask<int32_t> {
        val = co_await counter(val);
        val = co_await counter(val);
        val = co_await counter(val);
        co_return val;
    };

    auto task2 = [&](int32_t val) -> tinycoro::SlimTask<int32_t> {
        val = co_await task1(val);
        co_return val;
    };

    auto task3 = [&](int32_t val) -> tinycoro::SlimTask<int32_t> {
        val = co_await task2(val);
        co_return val;
    };

    auto val = tinycoro::AllOf(task3(0));

    EXPECT_EQ(val, 3);
}