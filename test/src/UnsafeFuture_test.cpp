#include <gtest/gtest.h>

#include <tinycoro/UnsafeFuture.hpp>

TEST(PromiseFutureTest, PromiseFutureTest_construct)
{
    // future needs to outlive promise
    std::vector<tinycoro::unsafe::Future<int32_t>> futures;
    {
        tinycoro::unsafe::Promise<int32_t> promise;
        futures.emplace_back(promise.get_future());
        promise.set_value(42);
    }

    EXPECT_EQ(futures[0].get(), 42);
}

TEST(PromiseFutureTest, PromiseFutureTest_set_2_times)
{
    // future needs to outlive promise
    std::vector<tinycoro::unsafe::Future<int32_t>> futures;
    {
        tinycoro::unsafe::Promise<int32_t> promise;
        futures.emplace_back(promise.get_future());
        promise.set_value(42);
        EXPECT_THROW(promise.set_value(44), tinycoro::FutureException);
    }

    EXPECT_EQ(futures[0].get(), 42);
}

TEST(PromiseFutureTest, PromiseFutureTest_set_exception_2_times)
{
    // future needs to outlive promise
    std::vector<tinycoro::unsafe::Future<int32_t>> futures;
    {
        tinycoro::unsafe::Promise<int32_t> promise;
        futures.emplace_back(promise.get_future());
        std::exception_ptr exceptionPtr;

        try
        {
            throw std::exception{};
        }
        catch(...)
        {
            exceptionPtr = std::current_exception();
        }
        

        promise.set_exception(exceptionPtr);
        EXPECT_THROW(promise.set_exception(exceptionPtr), tinycoro::FutureException);
    }

    EXPECT_THROW(futures[0].get(), std::exception);
}

TEST(PromiseFutureTest, PromiseFutureTest_set_2_times_OnlyMoveConstructable)
{
    struct OnlyMoveConstructable
    {
        OnlyMoveConstructable(int32_t val)
        : value{val}
        {
        }

        OnlyMoveConstructable(OnlyMoveConstructable&&) = default;

        int32_t value{};
    };

    // future needs to outlive promise
    std::vector<tinycoro::unsafe::Future<OnlyMoveConstructable>> futures;
    {
        tinycoro::unsafe::Promise<OnlyMoveConstructable> promise;
        futures.emplace_back(promise.get_future());
        promise.set_value(42);
        EXPECT_THROW(promise.set_value(44), tinycoro::FutureException);
    }

    EXPECT_EQ(futures[0].get().value, 42);
}