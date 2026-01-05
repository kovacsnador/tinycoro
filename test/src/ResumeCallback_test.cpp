#include <gtest/gtest.h>

#include <tinycoro/Common.hpp>
#include <tinycoro/ResumeCallback.hpp>

TEST(ResumeCallbackTest, ResumeCallback_concept)
{
    using function_t = void (*)(void*, void*, tinycoro::ENotifyPolicy);
    using resumeCallback_t = tinycoro::detail::ResumeCallback<function_t, void*, void*>;

    EXPECT_TRUE((tinycoro::concepts::IsResumeCallbackType<resumeCallback_t>));
}

TEST(ResumeCallbackTest, ResumeCallbackTest_default_constructor)
{
    using function_t = void (*)(void*, void*, bool);

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{};
 
    EXPECT_FALSE(func);

    auto func2 = func;

    EXPECT_FALSE(func2);

    auto func3 = std::move(func2);

    EXPECT_FALSE(func3);
}

TEST(ResumeCallbackTest, ResumeCallback_copy_constructor)
{
    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    func(true);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);

    decltype(func) func2{func};

    EXPECT_TRUE(func);
    EXPECT_TRUE(func2);

    func2(true);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 2);
}

TEST(ResumeCallbackTest, ResumeCallback_copy_assign)
{
    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    func(true);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);

    decltype(func) func2{};
    func2 = func;

    EXPECT_TRUE(func);
    EXPECT_TRUE(func2);

    func2(true);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 2);
}

TEST(ResumeCallbackTest, ResumeCallback_move_constructor)
{
    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    func(true);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);

    decltype(func) func2{std::move(func)};

    EXPECT_TRUE(func2);

    func2(true);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 2);
}

TEST(ResumeCallbackTest, ResumeCallback_move_assign)
{
    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    func(true);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);

    decltype(func) func2{};
    func2 = std::move(func);

    EXPECT_TRUE(func2);

    func2(true);
    EXPECT_EQ(a, 2);
    EXPECT_EQ(b, 2);
}

TEST(ResumeCallbackTest, ResumeCallback_without_return_value)
{
    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    func(true);

    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

TEST(ResumeCallbackTest, ResumeCallback_with_return_value)
{
    using function_t = bool(*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);

        return !c;
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    auto ret = func(true);

    EXPECT_FALSE(ret);
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 1);
}

/* struct ResumeCallbackTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(ResumeCallbackTest, ResumeCallbackTest, testing::Values(10, 100, 1000, 10000, 100000, 1000000, 10000000, 30000000));

TEST_P(ResumeCallbackTest, ResumeCallback_benchmark)
{
    const auto count = GetParam();

    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    for (size_t i = 0; i < count; ++i)
    {
        tinycoro::detail::ResumeCallback<function_t, void*, void*> func_copy = func;
        func(true);
        func_copy = {}; 
    }

    EXPECT_EQ(a, count);
    EXPECT_EQ(b, count);
}

TEST_P(ResumeCallbackTest, std_function_benchmark)
{
    const auto count = GetParam();

    int32_t a{};
    int32_t b{};

    std::function<void(bool)> func{[&a, &b](bool c) {
        a++;
        b++;
        EXPECT_TRUE(c);
    }};

    for (size_t i = 0; i < count; ++i)
    {
        std::function<void(bool)> func_copy = func;
        func(true);
        func_copy = {};
    }

    EXPECT_EQ(a, count);
    EXPECT_EQ(b, count);
}

TEST_P(ResumeCallbackTest, ResumeCallback_copy_benchmark)
{
    const auto count = GetParam();

    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    for (size_t i = 0; i < count; ++i)
    {
        tinycoro::detail::ResumeCallback<function_t, void*, void*> func_copy = func;
        func_copy = {};
    }
}

TEST_P(ResumeCallbackTest, std_function_copy_benchmark)
{
    const auto count = GetParam();

    int32_t a{};
    int32_t b{};

    std::function<void(bool)> func{[&a, &b](bool c) {
        a++;
        b++;
        EXPECT_TRUE(c);
    }};

    for (size_t i = 0; i < count; ++i)
    {
        std::function<void(bool)> func_copy = func;
        func_copy = {};
    }
}

TEST_P(ResumeCallbackTest, ResumeCallback_invoke_benchmark)
{
    const auto count = GetParam();

    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    for (size_t i = 0; i < count; ++i)
    {
        func(true);
    }

    EXPECT_EQ(a, count);
    EXPECT_EQ(b, count);
}

TEST_P(ResumeCallbackTest, ResumeCallback_std_invoke_benchmark)
{
    const auto count = GetParam();

    using function_t = void (*)(void*, void*, bool);

    auto f = [](void* a, void* b, bool c) {
        (*static_cast<int32_t*>(a))++;
        (*static_cast<int32_t*>(b))++;
        EXPECT_TRUE(c);
    };

    int32_t a{};
    int32_t b{};

    tinycoro::detail::ResumeCallback<function_t, void*, void*> func{f, &a, &b};

    for (size_t i = 0; i < count; ++i)
    {
        std::invoke(func, true);
    }

    EXPECT_EQ(a, count);
    EXPECT_EQ(b, count);
}

TEST_P(ResumeCallbackTest, std_function_invoke_benchmark)
{
    const auto count = GetParam();

    int32_t a{};
    int32_t b{};

    std::function<void(bool)> func{[&a, &b](bool c) {
        a++;
        b++;
        EXPECT_TRUE(c);
    }};

    for (size_t i = 0; i < count; ++i)
    {
        func(true);
    }

    EXPECT_EQ(a, count);
    EXPECT_EQ(b, count);
}*/
