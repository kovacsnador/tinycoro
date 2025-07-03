#include <gtest/gtest.h>

#include "tinycoro/tinycoro_all.h"

#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ == 11)
#define TINY_CORO_GCC_11
#endif

struct YieldValueTest : testing::TestWithParam<int32_t>
{
};

INSTANTIATE_TEST_SUITE_P(YieldValueTest, YieldValueTest, testing::Values(1, 10, 100));

TEST_P(YieldValueTest, YieldValueTest_generator)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    auto generator = [](int32_t max) -> tinycoro::Task<int32_t> {
        for (auto it : std::views::iota(0, max))
        {
            co_yield it;
        }
        co_return max;
    };

    auto consumer = [gen = generator](int32_t max) -> tinycoro::Task<void> {
        auto task = gen(max);
        for (auto it : std::views::iota(0, max))
        {
            EXPECT_EQ(co_await task, it);
        }

        EXPECT_EQ(co_await task, max);
    };

    tinycoro::AllOf(scheduler, consumer(count));
}

// Disable code becasue of GCC-11 bug.
//
// The compiler error message was to following on the pipeline:
// tinycoro/test/src/YieldValue_test.cpp:53:20: error: no suspend point info for ''co_await' not supported by dump_decl<declaration error>'
//
// See: https://stackoverflow.com/questions/67860049/why-cant-co-await-return-a-string
#ifndef TINY_CORO_GCC_11

tinycoro::Task<std::variant<int32_t, bool>> YieldCoroutine()
{
    co_yield 41;
    co_return true;
}

TEST(YieldValueTest, YieldValueTest_variant_yield)
{
    tinycoro::Scheduler scheduler;

    auto runner = []() -> tinycoro::Task<void> {
        auto task = YieldCoroutine();

        auto val = co_await task;

        EXPECT_EQ(std::get<0>(val), 41);

        val = co_await task;

        EXPECT_EQ(std::get<bool>(val), true);
    };

    tinycoro::AllOf(scheduler, runner());
}

TEST(YieldValueTest, YieldValueTest_variant_yield_runinline)
{
    auto runner = []() -> tinycoro::Task<void> {
        auto task = YieldCoroutine();

        auto val = co_await task;

        EXPECT_EQ(std::get<0>(val), 41);

        val = co_await task;

        EXPECT_EQ(std::get<bool>(val), true);
    };

    tinycoro::AllOfInline(runner());
}

#endif /* !TINY_CORO_GCC_11 */

TEST(YieldValueTest, YieldValueTest_direct_invoke_runinline)
{
    auto func = []() -> tinycoro::Task<std::unique_ptr<int32_t>> {
        co_yield std::make_unique<int32_t>(41);
        co_return std::make_unique<int32_t>(42);
    };

    // in this case co_yield is just ignored
    auto val = tinycoro::AllOfInline(func());
    EXPECT_EQ(*(val.value()), 42);
}

TEST(YieldValueTest, YieldValueTest_direct_invoke_scheduler)
{
    tinycoro::Scheduler scheduler;

    auto func = []() -> tinycoro::Task<std::unique_ptr<int32_t>> {
        co_yield std::make_unique<int32_t>(41);
        co_return std::make_unique<int32_t>(42);
    };

    // in this case co_yield is just ignored
    auto val = tinycoro::AllOf(scheduler, func());
    EXPECT_EQ(*(val.value()), 42);
}