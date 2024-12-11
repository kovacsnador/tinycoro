#include <gtest/gtest.h>

#include <concepts>

#include <tinycoro/Promise.hpp>

TEST(PromiseTest, PromiseTest_void)
{
    tinycoro::Promise<void> promise;

    EXPECT_TRUE(requires { promise.return_void(); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });
    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<tinycoro::FinalAwaiter, decltype(promise.final_suspend())>));

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));
}

TEST(PromiseTest, PromiseTest_int)
{
    tinycoro::Promise<int32_t> promise;
    EXPECT_TRUE(requires { promise.return_value(42); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });

    promise.return_value(42);

    EXPECT_EQ(promise.ReturnValue(), 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<tinycoro::FinalAwaiter, decltype(promise.final_suspend())>));
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

    auto obj = promise.ReturnValue();

    EXPECT_EQ(obj.i, 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<tinycoro::FinalAwaiter, decltype(promise.final_suspend())>));
}

struct FinalAwaiterMock
{
    [[nodiscard]] bool                    await_ready() const noexcept { return false; }
    [[nodiscard]] std::coroutine_handle<> await_suspend([[maybe_unused]] auto hdl) noexcept { return std::noop_coroutine(); }
    void                                  await_resume() const noexcept { }
};

TEST(PromiseTest, PromiseTest_FinalAwaiter)
{
    tinycoro::PromiseT<FinalAwaiterMock, tinycoro::PromiseReturnValue<int32_t>, tinycoro::PauseHandler, std::stop_source> promise;
    EXPECT_TRUE(requires { promise.return_value(42); });
    EXPECT_TRUE(requires { typename decltype(promise)::value_type; });

    promise.return_value(42);

    EXPECT_EQ(promise.ReturnValue(), 42);

    EXPECT_TRUE((std::same_as<std::coroutine_handle<decltype(promise)>, decltype(promise.get_return_object())>));

    EXPECT_TRUE((std::same_as<std::suspend_always, decltype(promise.initial_suspend())>));
    EXPECT_TRUE((std::same_as<FinalAwaiterMock, decltype(promise.final_suspend())>));
}