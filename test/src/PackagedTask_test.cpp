#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>

#include <tinycoro/PackagedTask.hpp>
#include <tinycoro/Future.hpp>

#include "mock/TaskMock.hpp"

struct Ex{};

ACTION(ThrowRuntimeError)
{
    throw std::runtime_error{"Runtime error!"};
}

template<typename FutureStateT>
struct PackagedTaskTest : public testing::Test
{
    using value_type = FutureStateT;
};

using PackagedTaskTestTypes = testing::Types<std::promise<int>, std::promise<void>, std::promise<Ex>, tinycoro::FutureState<int>, tinycoro::FutureState<void>, tinycoro::FutureState<Ex>>;

TYPED_TEST_SUITE(PackagedTaskTest, PackagedTaskTestTypes);

TYPED_TEST(PackagedTaskTest, PackagedTaskTest_int)
{
    using PromiseT = TestFixture::value_type;
    using ValueT = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT promise;
    auto future = promise.get_future();
    {
        tinycoro::test::TaskMock<ValueT> task;

        if constexpr (std::same_as<ValueT, Ex>)
        {
            EXPECT_CALL(*task.mock, Resume()).Times(1).WillOnce(ThrowRuntimeError());
        }
        else
        {
            // Setting up expectations for the mock methods
            EXPECT_CALL(*task.mock, Resume()).Times(1).WillOnce(::testing::Return(tinycoro::ETaskResumeState::DONE));
        }

        if constexpr (std::same_as<ValueT, int32_t>)
        {
            EXPECT_CALL(*task.mock, await_resume()).Times(1).WillOnce(::testing::Return(42)); // Return any value you'd expect
        }
        else
        {
            EXPECT_CALL(*task.mock, await_resume()).Times(0);
        }

        EXPECT_CALL(*task.mock, IsPaused()).Times(1).WillOnce(::testing::Return(false));

        tinycoro::PackagedTask packagedTask(std::move(task), std::move(promise), 0);

        EXPECT_FALSE(packagedTask.IsPaused());
        EXPECT_EQ(packagedTask(), tinycoro::ETaskResumeState::DONE);
    }

    if constexpr (std::same_as<ValueT, int32_t>)
    {
        EXPECT_EQ(future.get(), 42);
    }
    else if constexpr (std::same_as<ValueT, void>)
    {
        EXPECT_NO_THROW(future.get());
    }
    else
    {
        auto futureGetter = [&future] {std::ignore = future.get(); };
        EXPECT_THROW(futureGetter(), std::runtime_error);
    }
}

template<typename T>
struct PackagedTaskTestException : public testing::Test
{
    using value_type = T;
};

using PackagedTaskTestExceptionTypes = testing::Types<std::promise<void>, tinycoro::FutureState<void>>;

TYPED_TEST_SUITE(PackagedTaskTestException, PackagedTaskTestExceptionTypes);

TYPED_TEST(PackagedTaskTestException, PackagedTaskTest_void_exception)
{
    using PromiseT = TestFixture::value_type;
    using ValueT = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT promise;
    auto future = promise.get_future();
    {
        tinycoro::test::TaskMock<ValueT> task;

        // Setting up expectations for the mock methods
        EXPECT_CALL(*task.mock, Resume()).Times(1).WillOnce(ThrowRuntimeError());

        EXPECT_CALL(*task.mock, await_resume()).Times(0); // Return any value you'd expect

        EXPECT_CALL(*task.mock, IsPaused()).Times(1).WillOnce(::testing::Return(true));

        tinycoro::PackagedTask packagedTask(std::move(task), std::move(promise), 0);

        EXPECT_TRUE(packagedTask.IsPaused());
        EXPECT_EQ(packagedTask(), tinycoro::ETaskResumeState::DONE);
    }

    EXPECT_THROW(future.get(), std::runtime_error);
}