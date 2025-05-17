#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

TEST(InlineTaskTest, InlineTaskTest)
{
    tinycoro::Scheduler scheduler;

    auto inlineTask1 = []() -> tinycoro::InlineTask<int32_t> {
        auto inlineTask2 = []() -> tinycoro::InlineTask<int32_t> { co_return 41; };

        auto val = co_await inlineTask2();
        co_return val += 1;
    };

    auto task = [&]() -> tinycoro::Task<void> {
        auto val = co_await inlineTask1();
        EXPECT_EQ(val, 42);
    };

    tinycoro::GetAll(scheduler, task());
}

TEST(InlineTaskTest, InlineTaskTest_run_inline)
{
    auto inlineTask1 = []() -> tinycoro::InlineTask<int32_t> {
        auto inlineTask2 = []() -> tinycoro::InlineTask<int32_t> { co_return 41; };

        auto val = co_await inlineTask2();
        co_return val += 1;
    };

    auto val = tinycoro::RunInline(inlineTask1());

    EXPECT_EQ(val, 42);
}