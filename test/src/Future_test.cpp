#include <gtest/gtest.h>

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

TEST(FutureStateTest, FutureTest_set)
{
    tinycoro::FutureState<void> futureState;

    futureState.set_value();
    EXPECT_THROW(futureState.set_value(), tinycoro::AssociatedStateStatisfiedException);
}

TEST(FutureStateTest, FutureStateTest_destructor)
{
    auto futureState = std::make_unique<tinycoro::FutureState<void>>();
    auto future = futureState->get_future();

    // destroy future state
    futureState.reset();

    EXPECT_THROW(future.get(), tinycoro::FutureStateException);
}

TEST(FutureTest, FutureTest_get)
{
    tinycoro::FutureState<void> futureState;

    auto future = futureState.get_future();

    futureState.set_value();
    EXPECT_THROW(futureState.set_value(), tinycoro::AssociatedStateStatisfiedException);

    EXPECT_TRUE(future.valid());
    future.get();
    EXPECT_FALSE(future.valid());
    future.get();
    EXPECT_FALSE(future.valid());
}