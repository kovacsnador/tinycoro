#include <gtest/gtest.h>

#include <vector>
#include <stdexcept>

#include <tinycoro/tinycoro_all.h>

TEST(InlineAwaitTest, InlineAwaitTest_tasks_int_return)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto awaitable = tinycoro::AllOfInlineAwait(task(40), task(41), task(42));

    EXPECT_TRUE(awaitable.await_ready());
    EXPECT_FALSE(awaitable.await_suspend(int32_t{}));

    auto [r1, r2, r3] = awaitable.await_resume();
    EXPECT_EQ(r1, 40);
    EXPECT_EQ(r2, 41);
    EXPECT_EQ(r3, 42);
}

TEST(InlineAwaitTest, InlineAwaitTest_tasks_int_return_single_task)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto awaitable = tinycoro::AllOfInlineAwait(task(42));

    EXPECT_TRUE(awaitable.await_ready());
    EXPECT_FALSE(awaitable.await_suspend(int32_t{}));

    auto r1 = awaitable.await_resume();
    EXPECT_EQ(r1, 42);
}

TEST(InlineAwaitTest, InlineAwaitTest_tasks_void_return)
{
    size_t count{0};

    auto task = [&]() -> tinycoro::InlineTask<> {
        count++;
        co_return;
    };

    auto awaitable = tinycoro::AllOfInlineAwait(task(), task(), task());

    EXPECT_TRUE(awaitable.await_ready());
    EXPECT_FALSE(awaitable.await_suspend(int32_t{}));

    awaitable.await_resume();
    EXPECT_EQ(count, 3);
}

TEST(InlineAwaitTest, InlineAwaitTest_container_int_return)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    std::vector<tinycoro::InlineTask<int32_t>> container;
    container.push_back(task(40));
    container.push_back(task(41));
    container.push_back(task(42));

    auto awaitable = tinycoro::AllOfInlineAwait(container);

    EXPECT_TRUE(awaitable.await_ready());
    EXPECT_FALSE(awaitable.await_suspend(int32_t{}));

    auto result = awaitable.await_resume();
    EXPECT_EQ(result[0], 40);
    EXPECT_EQ(result[1], 41);
    EXPECT_EQ(result[2], 42);
}

TEST(InlineAwaitTest, InlineAwaitTest_container_void_return)
{
    size_t count{0};

    auto task = [&]() -> tinycoro::InlineTask<> {
        count++;
        co_return;
    };

    std::vector<tinycoro::InlineTask<>> container{};
    container.push_back(task());
    container.push_back(task());
    container.push_back(task());

    auto awaitable = tinycoro::AllOfInlineAwait(container);

    EXPECT_TRUE(awaitable.await_ready());
    EXPECT_FALSE(awaitable.await_suspend(int32_t{}));

    awaitable.await_resume();
    EXPECT_EQ(count, 3);
}

TEST(InlineAwaitTest, InlineAwaitTest_coawait)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto wrapper = [&]() -> tinycoro::InlineTask<int32_t> {
        auto [r1, r2, r3] = co_await tinycoro::AllOfInlineAwait(task(40), task(41), task(42));
        co_return *r1 + *r2 + *r3;
    };

    auto result = tinycoro::AllOfInline(wrapper());

    EXPECT_EQ(123, result);
}

TEST(InlineAwaitTest, InlineAwaitTest_coawait_exception)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> {
        throw std::runtime_error{"error"};
        co_return val;
    };

    auto wrapper = [&]() -> tinycoro::InlineTask<int32_t> {
        auto [r1, r2, r3] = co_await tinycoro::AllOfInlineAwait(task(40), task(41), task(42));
        co_return *r1 + *r2 + *r3;
    };

    auto func = [&] { std::ignore = tinycoro::AllOfInline(wrapper()); };
    EXPECT_THROW(func(), std::runtime_error);
}

TEST(InlineAwaitTest, InlineAwaitTest_coawait_container)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto wrapper = [&]() -> tinycoro::InlineTask<int32_t> {
        std::vector<tinycoro::InlineTask<int32_t>> vec;
        vec.push_back(task(40));
        vec.push_back(task(41));
        vec.push_back(task(42));

        auto res = co_await tinycoro::AllOfInlineAwait(vec);
        co_return res[0].value() + res[1].value() + res[2].value();
    };

    auto result = tinycoro::AllOfInline(wrapper());

    EXPECT_EQ(123, result);
}

TEST(InlineAwaitTest, InlineAwaitTest_coawait_container_void)
{
    size_t count{0};

    auto task = [](size_t& c) -> tinycoro::InlineTask<> {
        c++;
        co_return;
    };

    auto wrapper = [&]() -> tinycoro::InlineTask<> {
        std::vector<tinycoro::InlineTask<>> vec;
        vec.push_back(task(count));
        vec.push_back(task(count));
        vec.push_back(task(count));

        co_await tinycoro::AllOfInlineAwait(vec);
        co_return;
    };

    tinycoro::AllOfInline(wrapper());

    EXPECT_EQ(3, count);
}

TEST(InlineAwaitTest, InlineAwaitTest_coawait_container_exception)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> {
        throw std::runtime_error{"error"};
        co_return val;
    };

    auto wrapper = [&]() -> tinycoro::InlineTask<int32_t> {
        std::vector<tinycoro::InlineTask<int32_t>> vec;
        vec.push_back(task(40));
        vec.push_back(task(41));
        vec.push_back(task(42));

        auto res = co_await tinycoro::AllOfInlineAwait(vec);
        co_return res[0].value() + res[1].value() + res[2].value();
    };

    auto func = [&] { std::ignore = tinycoro::AllOfInline(wrapper()); };
    EXPECT_THROW(func(), std::runtime_error);
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto awaiter = tinycoro::AnyOfInlineAwait(task(42));
    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_FALSE(awaiter.await_suspend(int32_t{}));

    auto res = awaiter.await_resume();
    EXPECT_EQ(res, 42);
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_stop_source)
{
    std::stop_source ss;
    auto             task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto awaiter = tinycoro::AnyOfInlineAwait(ss, task(40), task(41), task(42));
    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_FALSE(awaiter.await_suspend(int32_t{}));

    auto [r1, r2, r3] = awaiter.await_resume();
    EXPECT_EQ(r1, 40);
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_stop_source_container)
{
    std::stop_source ss;
    auto             task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    std::vector<tinycoro::InlineTask<int32_t>> vec;
    vec.push_back(task(40));
    vec.push_back(task(41));
    vec.push_back(task(42));

    auto awaiter = tinycoro::AnyOfInlineAwait(ss, vec);
    EXPECT_TRUE(awaiter.await_ready());
    EXPECT_FALSE(awaiter.await_suspend(int32_t{}));

    auto res = awaiter.await_resume();
    EXPECT_EQ(res[0], 40);
    EXPECT_FALSE(res[1].has_value());
    EXPECT_FALSE(res[2].has_value());
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_functional_test)
{
    auto task = [](int32_t val) -> tinycoro::InlineTask<int32_t> { co_return val; };

    auto wrapper = [&]() -> tinycoro::InlineTask<> {
        auto [r1, r2, r3] = co_await tinycoro::AnyOfInlineAwait(task(40), task(41), task(42));

        EXPECT_EQ(r1, 40);
        EXPECT_FALSE(r2.has_value());
        EXPECT_FALSE(r3.has_value());

        co_return;
    };

    tinycoro::AllOfInline(wrapper());
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_functional_test_stopSource)
{
    std::stop_source stopSource{};

    auto task = [&stopSource](int32_t val) -> tinycoro::InlineTask<int32_t> {
        auto ss = co_await tinycoro::this_coro::stop_source();
        ss.request_stop();

        EXPECT_EQ(ss, stopSource);

        co_return val;
    };

    auto wrapper = [&]() -> tinycoro::InlineTask<> {
        auto [r1, r2, r3] = co_await tinycoro::AnyOfInlineAwait(stopSource, task(40), task(41), task(42));

        EXPECT_EQ(r1, 40);
        EXPECT_FALSE(r2.has_value());
        EXPECT_FALSE(r3.has_value());

        co_return;
    };

    tinycoro::AllOfInline(wrapper());
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_functional_test_stopSource_vector)
{
    std::stop_source stopSource{};

    auto task = [&stopSource](int32_t val) -> tinycoro::InlineTask<int32_t> {
        auto ss = co_await tinycoro::this_coro::stop_source();
        ss.request_stop();

        EXPECT_EQ(ss, stopSource);

        co_return val;
    };

    std::vector<tinycoro::InlineTask<int32_t>> tasks;
    tasks.push_back(task(42));
    tasks.push_back(task(2));
    tasks.push_back(task(3));

    auto wrapper = [&]() -> tinycoro::InlineTask<> {
        auto res = co_await tinycoro::AnyOfInlineAwait(stopSource, tasks);

        EXPECT_EQ(res[0], 42);
        EXPECT_FALSE(res[1].has_value());
        EXPECT_FALSE(res[2].has_value());
    };

    tinycoro::AllOfInline(wrapper());
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_functional_test_InlineNIC)
{
    std::stop_source stopSource{};

    auto task = [&stopSource](int32_t val) -> tinycoro::InlineTaskNIC<int32_t> {
        auto ss = co_await tinycoro::this_coro::stop_source();
        EXPECT_EQ(ss, stopSource);

        co_return val;
    };

    auto wrapper = [&]() -> tinycoro::InlineTask<> {
        // those tasks are not initial cancellable.
        // So they will just simply return
        auto [r1, r2, r3] = co_await tinycoro::AnyOfInlineAwait(stopSource, task(40), task(41), task(42));

        EXPECT_EQ(r1, 40);
        EXPECT_EQ(r2, 41);
        EXPECT_EQ(r3, 42);
    };

    tinycoro::AllOfInline(wrapper());
}

TEST(AnyOfInlineAwaitTest, AnyOfInlineAwaitTest_functional_test_InlineNIC_vector)
{
    std::stop_source stopSource{};

    auto task = [&stopSource](int32_t val) -> tinycoro::InlineTaskNIC<int32_t> {
        auto ss = co_await tinycoro::this_coro::stop_source();
        EXPECT_EQ(ss, stopSource);

        co_return val;
    };

    std::vector<tinycoro::InlineTaskNIC<int32_t>> tasks;
    tasks.push_back(task(40));
    tasks.push_back(task(41));
    tasks.push_back(task(42));

    auto wrapper = [&]() -> tinycoro::InlineTask<> {
        // those tasks are not initial cancellable.
        // So they will just simply return
        auto res = co_await tinycoro::AnyOfInlineAwait(stopSource, tasks);

        EXPECT_EQ(res[0], 40);
        EXPECT_EQ(res[1], 41);
        EXPECT_EQ(res[2], 42);
    };

    tinycoro::AllOfInline(wrapper());
}