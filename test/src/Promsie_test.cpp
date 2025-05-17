#include <gtest/gtest.h>

#include <concepts>

#include <tinycoro/Promise.hpp>

TEST(PromiseTest, PromiseTest_void)
{
    tinycoro::Promise<void> promise;

    EXPECT_TRUE(requires { promise.return_void(); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });
    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<tinycoro::detail::FinalAwaiter, decltype(promise.final_suspend())>));

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));
}

TEST(PromiseTest, PromiseTest_int)
{
    tinycoro::Promise<int32_t> promise;
    EXPECT_TRUE(requires { promise.return_value(42); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });

    promise.return_value(42);

    EXPECT_EQ(promise.value(), 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<tinycoro::detail::FinalAwaiter, decltype(promise.final_suspend())>));
}

TEST(PromiseTest, PromiseTest_MoveOnly)
{
    struct MoveOnly
    {
        MoveOnly(int32_t v)
        : i{v}
        {
        }

        MoveOnly(MoveOnly&&)            = default;
        MoveOnly& operator=(MoveOnly&&) = default;

        int32_t i{};
    };

    tinycoro::Promise<MoveOnly> promise;

    EXPECT_TRUE(requires { promise.return_value(MoveOnly{12}); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });

    promise.return_value(MoveOnly{42});

    auto obj = promise.value();

    EXPECT_EQ(obj.i, 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<tinycoro::detail::FinalAwaiter, decltype(promise.final_suspend())>));
}

struct FinalAwaiterMock
{
    [[nodiscard]] bool                    await_ready() const noexcept { return false; }
    [[nodiscard]] std::coroutine_handle<> await_suspend([[maybe_unused]] auto hdl) noexcept { return std::noop_coroutine(); }
    void                                  await_resume() const noexcept { }
};

TEST(PromiseTest, PromiseTest_FinalAwaiter)
{
    tinycoro::detail::PromiseT<FinalAwaiterMock, tinycoro::detail::PromiseReturnValue<int32_t, FinalAwaiterMock>, tinycoro::PauseHandler, std::stop_source> promise;
    EXPECT_TRUE(requires { promise.return_value(42); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });

    promise.return_value(42);

    EXPECT_EQ(promise.value(), 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<FinalAwaiterMock, decltype(promise.final_suspend())>));
}

TEST(PromiseTest, PromiseTest_YieldValue)
{
    tinycoro::detail::PromiseT<FinalAwaiterMock, tinycoro::detail::PromiseReturnValue<int32_t, FinalAwaiterMock>, tinycoro::PauseHandler, std::stop_source> promise;
    EXPECT_TRUE(requires { promise.return_value(42); });
    EXPECT_TRUE(requires { promise.yield_value(42); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });

    auto yieldAwaiter = promise.yield_value(41);
    EXPECT_TRUE((std::same_as<FinalAwaiterMock, decltype(yieldAwaiter)>));

    EXPECT_EQ(promise.value(), 41);

    promise.return_value(42);
    EXPECT_EQ(promise.value(), 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<FinalAwaiterMock, decltype(promise.final_suspend())>));
}

TEST(PromiseTest, PromiseTest_reference)
{
    struct S
    {
        int32_t i{};
    };

    S s{42};

    tinycoro::detail::PromiseReturnValue<S&, FinalAwaiterMock> promise;

    promise.return_value(s);
    auto& s_ref = promise.value();

    EXPECT_EQ(s_ref.i, s.i);
    EXPECT_EQ(std::addressof(s), std::addressof(s_ref));

    S s2{44};

    promise.yield_value(s2);

    auto& s_ref2 = promise.value();

    EXPECT_EQ(s_ref2.i, s2.i);
    EXPECT_EQ(std::addressof(s2), std::addressof(s_ref2));
}

TEST(PromiseTest, PromiseTest_pointer)
{
    struct S
    {
        int32_t i{};
    };

    S s{42};

    tinycoro::detail::PromiseReturnValue<S*, FinalAwaiterMock> promise;

    promise.return_value(std::addressof(s));
    auto s_ptr = promise.value();

    EXPECT_EQ(s_ptr->i, s.i);
    EXPECT_EQ(std::addressof(s), std::addressof(*s_ptr));

    S s2{44};

    promise.yield_value(std::addressof(s2));

    auto& s_ptr2 = promise.value();

    EXPECT_EQ(s_ptr2->i, s2.i);
    EXPECT_EQ(std::addressof(s2), std::addressof(*s_ptr2));
}

TEST(PromiseTest, PromiseTest_not_trivial)
{
    struct S
    {
        std::string i{};
    };

    S s{"42"};

    tinycoro::detail::PromiseReturnValue<S, FinalAwaiterMock> promise;

    promise.return_value(s);

    auto s_copy = promise.value();
    EXPECT_EQ(s_copy.i, s.i);

    S s2{"44"};

    promise.yield_value(s2);

    auto&& s_copy2 = promise.value();
    EXPECT_EQ(s_copy2.i, s2.i);
}