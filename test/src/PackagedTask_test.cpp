#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory_resource>
#include <future>

#include <tinycoro/PackagedTask.hpp>
#include <tinycoro/UnsafeFuture.hpp>

#include "mock/TaskMock.hpp"

struct Ex
{
};

ACTION(ThrowRuntimeError)
{
    throw std::runtime_error{"Runtime error!"};
}

template <typename FutureStateT>
struct PackagedTaskTest : public testing::Test
{
    using value_type = FutureStateT;
};

using PackagedTaskTestTypes = testing::Types<std::promise<std::optional<int32_t>>,
                                             std::promise<std::optional<tinycoro::VoidType>>,
                                             std::promise<std::optional<Ex>>,
                                             tinycoro::unsafe::Promise<std::optional<int32_t>>,
                                             tinycoro::unsafe::Promise<std::optional<tinycoro::VoidType>>,
                                             tinycoro::unsafe::Promise<std::optional<Ex>>>;

TYPED_TEST_SUITE(PackagedTaskTest, PackagedTaskTestTypes);

template<typename T>
struct GetType;

template<>
struct GetType<std::optional<tinycoro::VoidType>>
{
    using value_type = void; 
};

template<typename T>
struct GetType<std::optional<T>>
{
    using value_type = T; 
};

TYPED_TEST(PackagedTaskTest, PackagedTaskTest_int)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    using TaskValueT = typename GetType<ValueT>::value_type;

    PromiseT promise;
    auto     future = promise.get_future();
    {
        tinycoro::test::TaskMock<TaskValueT> task;

        if constexpr (std::same_as<TaskValueT, Ex>)
        {
            EXPECT_CALL(*task.mock, Resume()).Times(1).WillOnce(ThrowRuntimeError());
        }
        else
        {
            // Setting up expectations for the mock methods
            EXPECT_CALL(*task.mock, Resume()).Times(1);
        }

        if constexpr (std::same_as<TaskValueT, int32_t>)
        {
            EXPECT_CALL(*task.mock, await_resume()).Times(1).WillOnce(::testing::Return(42)); // Return any value you'd expect
        }
        else
        {
            EXPECT_CALL(*task.mock, await_resume()).Times(0);
        }

        EXPECT_CALL(*task.mock, IsDone).Times(::testing::AnyNumber());

        std::pmr::polymorphic_allocator<std::byte> allocator;

        auto packedTask = tinycoro::detail::MakeSchedulableTask(std::move(task), std::move(promise), allocator);

        packedTask->Resume();
    }

    if constexpr (std::same_as<TaskValueT, int32_t>)
    {
        EXPECT_EQ(future.get(), 42);
    }
    else if constexpr (std::same_as<TaskValueT, void>)
    {
        EXPECT_NO_THROW(std::ignore = future.get());
    }
    else
    {
        auto futureGetter = [&future] { std::ignore = future.get(); };
        EXPECT_THROW(futureGetter(), std::runtime_error);
    }
}

template <typename T>
struct PackagedTaskTestException : public testing::Test
{
    using value_type = T;
};

using PackagedTaskTestExceptionTypes = testing::Types<std::promise<std::optional<tinycoro::VoidType>>, tinycoro::unsafe::Promise<std::optional<tinycoro::VoidType>>>;

TYPED_TEST_SUITE(PackagedTaskTestException, PackagedTaskTestExceptionTypes);

TYPED_TEST(PackagedTaskTestException, PackagedTaskTest_void_exception)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT promise;
    auto     future = promise.get_future();
    {
        tinycoro::test::TaskMock<typename GetType<ValueT>::value_type> task;

        // Setting up expectations for the mock methods
        EXPECT_CALL(*task.mock, Resume()).Times(1).WillOnce(ThrowRuntimeError());

        EXPECT_CALL(*task.mock, await_resume()).Times(0); // Return any value you'd expect

        std::pmr::polymorphic_allocator<std::byte> allocator;

        auto packedTask = tinycoro::detail::MakeSchedulableTask(std::move(task), std::move(promise), allocator);

        packedTask->Resume();
    }

    EXPECT_THROW(std::ignore = future.get(), std::runtime_error);
}