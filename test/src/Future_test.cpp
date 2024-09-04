#include <gtest/gtest.h>

#include <future>

#include <tinycoro/Future.hpp>

TEST(FutureTest, FutureTestSimpleCase)
{
    tinycoro::FutureState<int> futureState;

    auto future = futureState.get_future();

    EXPECT_TRUE(future.valid());

    int32_t value = 42; 

    futureState.set_value(value);
    EXPECT_EQ(future.get(), value);

    EXPECT_FALSE(future.valid());
}

TEST(FutureTest, FutureTestValidTest)
{
    std::promise<int> promise;

    auto f = promise.get_future();

    EXPECT_TRUE(f.valid());

    promise.set_value(42);

    EXPECT_TRUE(f.valid());

    std::ignore = f.get();

    EXPECT_FALSE(f.valid());


    tinycoro::FutureState<int> futureState;

    auto future1 = futureState.get_future();
    auto future2 = futureState.get_future();
    auto future3 = futureState.get_future();
    auto future4 = futureState.get_future();

    EXPECT_TRUE(future1.valid());
    EXPECT_TRUE(future2.valid());
    EXPECT_TRUE(future3.valid());
    EXPECT_TRUE(future4.valid());

    int32_t value = 42; 

    futureState.set_value(value);

    EXPECT_TRUE(future1.valid());
    EXPECT_TRUE(future2.valid());
    EXPECT_TRUE(future3.valid());
    EXPECT_TRUE(future4.valid());

    EXPECT_EQ(future1.get(), value);
    
    EXPECT_FALSE(future1.valid());
    EXPECT_FALSE(future2.valid());
    EXPECT_FALSE(future3.valid());
    EXPECT_FALSE(future4.valid());
}