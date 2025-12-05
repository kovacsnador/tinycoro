#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>
#include <utility>
#include <concepts>

#include <tinycoro/UnsafeFuture.hpp>
#include <tinycoro/Wait.hpp>

#include "tinycoro/tinycoro_all.h"

template <typename T>
class TD;

template <typename T>
struct GetAllTest : testing::Test
{
    using value_type = T;
};

template <typename t>
class TD;

using GetAllTestTypes = testing::Types<std::promise<std::optional<int32_t>>,
                                       tinycoro::unsafe::Promise<std::optional<int32_t>>,
                                       std::promise<std::optional<tinycoro::VoidType>>,
                                       tinycoro::unsafe::Promise<std::optional<tinycoro::VoidType>>>;

TYPED_TEST_SUITE(GetAllTest, GetAllTestTypes);

TYPED_TEST(GetAllTest, GetAllTest_one_Future)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT promise;
    auto     future = promise.get_future();

    if constexpr (std::same_as<std::optional<int32_t>, ValueT>)
    {
        promise.set_value(42);
        auto value = tinycoro::GetAll(future);
        EXPECT_EQ(value, 42);
    }
    else
    {
        promise.set_value(tinycoro::VoidType{});
        EXPECT_NO_THROW(tinycoro::GetAll(future));
    }
}

TYPED_TEST(GetAllTest, GetAllTest_one_Future_exception)
{
    using PromiseT = TestFixture::value_type;

    PromiseT promise;
    auto     future = promise.get_future();

    promise.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));

    if constexpr (requires { std::ignore = tinycoro::GetAll(future); })
    {
        auto getAll = [&future] { std::ignore = tinycoro::GetAll(future); };
        EXPECT_THROW(getAll(), std::runtime_error);
    }
    else
    {
        EXPECT_THROW(tinycoro::GetAll(future), std::runtime_error);
    }
}

TYPED_TEST(GetAllTest, GetAllTest_tuple)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT p1;
    PromiseT p2;
    PromiseT p3;

    auto tuple = std::make_tuple(p1.get_future(), p2.get_future(), p3.get_future());

    if constexpr (std::same_as<std::optional<int32_t>, ValueT>)
    {
        p1.set_value(40);
        p2.set_value(41);
        p3.set_value(42);

        auto [r1, r2, r3] = tinycoro::GetAll(tuple);

        EXPECT_EQ(*r1, 40);
        EXPECT_EQ(*r2, 41);
        EXPECT_EQ(*r3, 42);
    }
    else
    {
        p1.set_value(tinycoro::VoidType{});
        p2.set_value(tinycoro::VoidType{});
        p3.set_value(tinycoro::VoidType{});

        EXPECT_NO_THROW(tinycoro::GetAll(tuple));
    }
}

TYPED_TEST(GetAllTest, GetAllTest_tuple_exception)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT p1;
    PromiseT p2;
    PromiseT p3;

    auto tuple = std::make_tuple(p1.get_future(), p2.get_future(), p3.get_future());

    if constexpr (std::same_as<std::optional<int32_t>, ValueT>)
    {
        p1.set_value(40);
        p2.set_value(41);
        p3.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));

        auto getAll = [&tuple] { std::ignore = tinycoro::GetAll(tuple); };
        EXPECT_THROW(getAll(), std::runtime_error);
    }
    else
    {
        p1.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));
        p2.set_value(tinycoro::VoidType{});
        p3.set_value(tinycoro::VoidType{});

        EXPECT_THROW(tinycoro::GetAll(tuple), std::runtime_error);
    }
}

TYPED_TEST(GetAllTest, GetAllTest_vector_exception)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT p1;
    PromiseT p2;
    PromiseT p3;

    std::vector<decltype(p1.get_future())> tasks;
    tasks.emplace_back(p1.get_future());
    tasks.emplace_back(p2.get_future());
    tasks.emplace_back(p3.get_future());

    if constexpr (std::same_as<std::optional<int32_t>, ValueT>)
    {
        p1.set_value(40);
        p2.set_value(41);
        p3.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));

        auto getAll = [&tasks] { std::ignore = tinycoro::GetAll(tasks); };
        EXPECT_THROW(getAll(), std::runtime_error);
    }
    else
    {
        p1.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));
        p2.set_value(tinycoro::VoidType{});
        p3.set_value(tinycoro::VoidType{});

        EXPECT_THROW(tinycoro::GetAll(tasks), std::runtime_error);
    }
}

TEST(GetAllTest, GetAllTest_mixedValues_exception)
{
    std::promise<std::optional<tinycoro::VoidType>> p1;
    std::promise<std::optional<int32_t>> p2;
    std::promise<std::optional<bool>> p3;

    auto tuple = std::make_tuple(p1.get_future(), p2.get_future(), p3.get_future());

    p1.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));
    p2.set_value(41);
    p3.set_value(true);

    auto getAll = [&tuple] { std::ignore = tinycoro::GetAll(tuple); };
    EXPECT_THROW(getAll(), std::runtime_error);
}

template <typename T>
struct GetAllTestWithTupleMixed : testing::Test
{
    using value_type = T;
};

using GetAllTestWithTupleMixedTypes = testing::Types<std::promise<std::optional<int32_t>>, std::promise<std::optional<tinycoro::VoidType>>>;

TYPED_TEST_SUITE(GetAllTestWithTupleMixed, GetAllTestWithTupleMixedTypes);

template <typename T>
struct GetAllTestWithTupleMixedFutureState : testing::Test
{
    using value_type = T;
};

using GetAllTestWithTupleMixedFutureStateTypes = testing::Types<tinycoro::unsafe::Promise<std::optional<int32_t>>, tinycoro::unsafe::Promise<std::optional<tinycoro::VoidType>>>;

TYPED_TEST_SUITE(GetAllTestWithTupleMixedFutureState, GetAllTestWithTupleMixedFutureStateTypes);

struct S
{
    bool    b{};
    int32_t i{};
};

template <template <typename> class BasePromiseT, typename PromiseT, typename ValueT>
void GetAllTestWithTupleMixedTestHelper()
{
    PromiseT p1;
    PromiseT p2;
    PromiseT p3;

    using Promise_S_t     = BasePromiseT<S>;
    using Promise_float_t = BasePromiseT<float>;

    Promise_S_t     p4;
    Promise_float_t p5;

    auto tuple = std::make_tuple(p1.get_future(), p2.get_future(), p3.get_future(), p4.get_future(), p5.get_future());

    if constexpr (std::same_as<std::optional<int32_t>, ValueT>)
    {
        p1.set_value(40);
        p2.set_value(41);
        p3.set_value(42);
    }
    else
    {
        p1.set_value(tinycoro::VoidType{});
        p2.set_value(tinycoro::VoidType{});
        p3.set_value(tinycoro::VoidType{});
    }

    p4.set_value(S{true, 42});
    p5.set_value(3.14f);

    auto results = tinycoro::GetAll(tuple);

    if constexpr (std::same_as<std::optional<int32_t>, ValueT>)
    {
        EXPECT_EQ(std::get<0>(results), 40);
        EXPECT_EQ(std::get<1>(results), 41);
        EXPECT_EQ(std::get<2>(results), 42);
    }
    else
    {
        EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, std::decay_t<decltype(std::get<0>(results))>>));
        EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, std::decay_t<decltype(std::get<1>(results))>>));
        EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, std::decay_t<decltype(std::get<2>(results))>>));
    }

    EXPECT_EQ(std::get<3>(results).b, true);
    EXPECT_EQ(std::get<3>(results).i, 42);
    EXPECT_EQ(std::get<4>(results), 3.14f);
}

TYPED_TEST(GetAllTestWithTupleMixed, GetAllTest_tuple_mixedTypes_std_promise)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    GetAllTestWithTupleMixedTestHelper<std::promise, PromiseT, ValueT>();
}

TYPED_TEST(GetAllTestWithTupleMixedFutureState, GetAllTest_tuple_mixedTypes_std_promise)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    GetAllTestWithTupleMixedTestHelper<tinycoro::unsafe::Promise, PromiseT, ValueT>();
}

template <typename ReturnT>
struct SchedulerTestMock
{
    template<typename T>
    struct MockPromiseT{};

    template<template<typename> class PromiseT = MockPromiseT>
    auto Enqueue(auto...)
    {
        return EnqueueMethod();
    }

    MOCK_METHOD(ReturnT, EnqueueMethod, ());
};

static constexpr auto dummyTask = []()->tinycoro::Task<> { co_return; };

template <typename T>
struct AnyOfTest : testing::Test
{
    using value_type = T;
};

using AnyOfTypes
    = testing::Types<std::promise<std::optional<int32_t>>, std::promise<std::optional<tinycoro::VoidType>>, tinycoro::unsafe::Promise<std::optional<int32_t>>, tinycoro::unsafe::Promise<std::optional<tinycoro::VoidType>>>;

TYPED_TEST_SUITE(AnyOfTest, GetAllTestWithTupleMixedTypes);

TYPED_TEST(AnyOfTest, AnyOfTest)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

   std::stop_source stopSource;

   auto cb = [] {
        PromiseT p1;
        PromiseT p2;
        PromiseT p3;

        if constexpr (std::same_as<std::optional<tinycoro::VoidType>, ValueT>)
        {
            p1.set_value(tinycoro::VoidType{});
            p2.set_value(tinycoro::VoidType{});
            p3.set_value(tinycoro::VoidType{});
        }
        else
        {
            p1.set_value(ValueT{});
            p2.set_value(ValueT{});
            p3.set_value(ValueT{});
        }

        return std::make_tuple(p1.get_future(), p2.get_future(), p3.get_future());
    };

    SchedulerTestMock<std::invoke_result_t<decltype(cb)>> scheduler;

    EXPECT_CALL(scheduler, EnqueueMethod).WillOnce(testing::Invoke(cb));


    if constexpr (requires { std::ignore = tinycoro::AnyOf(scheduler, stopSource, dummyTask()); })
    {
        auto results = tinycoro::AnyOf(scheduler, stopSource, dummyTask());
        EXPECT_EQ(std::get<0>(results), ValueT{});
        EXPECT_EQ(std::get<1>(results), ValueT{});
        EXPECT_EQ(std::get<2>(results), ValueT{});
    }
    else
    {
        EXPECT_NO_THROW(tinycoro::AnyOf(scheduler, stopSource, dummyTask()));
    }
}

TEST(AnyOfTest, AnyOfTest_exception)
{
    auto cb = [] {
        std::promise<std::optional<tinycoro::VoidType>> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error{"Error"}));
        return std::make_tuple(promise.get_future());
    };

    SchedulerTestMock<std::invoke_result_t<decltype(cb)>> scheduler;

    EXPECT_CALL(scheduler, EnqueueMethod).WillOnce(testing::Invoke(cb));
    EXPECT_THROW(tinycoro::AnyOf(scheduler, dummyTask()), std::runtime_error);
}

TEST(AnyOfTest, AnyOfTest_single_int32_t)
{
    auto cb = [] {
        std::promise<std::optional<int32_t>> promise;
        promise.set_value(42);
        return std::make_tuple(promise.get_future());
    };

    SchedulerTestMock<std::invoke_result_t<decltype(cb)>> scheduler;

    EXPECT_CALL(scheduler, EnqueueMethod).WillOnce(testing::Invoke(cb));

    auto result = tinycoro::AnyOf(scheduler, dummyTask());

    EXPECT_TRUE((std::same_as<std::optional<int32_t>, decltype(result)>));
    EXPECT_EQ(result, 42);
}

TEST(AnyOfTest, AnyOfTest_single_int32_t_exception)
{
    auto cb = [] {
        std::promise<std::optional<int32_t>> promise;
        promise.set_exception(std::make_exception_ptr(std::runtime_error{"Error"}));
        return std::make_tuple(promise.get_future());
    };

    SchedulerTestMock<std::invoke_result_t<decltype(cb)>> scheduler;

    EXPECT_CALL(scheduler, EnqueueMethod).WillOnce(testing::Invoke(cb));

    EXPECT_THROW([&scheduler]{std::ignore = tinycoro::AnyOf(scheduler, dummyTask());}(), std::runtime_error);
}

TEST(AnyOfTest, AnyOfTest_multi_int32_t)
{
    auto cb = [] {
        std::promise<std::optional<int32_t>> p1;
        p1.set_value(42);

        std::promise<std::optional<int32_t>> p2;
        p2.set_value(43);

        std::promise<std::optional<tinycoro::VoidType>> p3;
        p3.set_value(tinycoro::VoidType{});

        return std::make_tuple(p1.get_future(), p2.get_future(), p3.get_future());
    };

    SchedulerTestMock<std::invoke_result_t<decltype(cb)>> scheduler;

    EXPECT_CALL(scheduler, EnqueueMethod).WillOnce(testing::Invoke(cb));

    auto results = tinycoro::AnyOf(scheduler, dummyTask());

    EXPECT_TRUE((std::same_as<std::optional<int32_t>, std::tuple_element_t<0, decltype(results)>>));
    EXPECT_TRUE((std::same_as<std::optional<int32_t>, std::tuple_element_t<1, decltype(results)>>));
    EXPECT_TRUE((std::same_as<std::optional<tinycoro::VoidType>, std::tuple_element_t<2, decltype(results)>>));

    EXPECT_EQ(std::get<0>(results).value(), 42);
    EXPECT_EQ(std::get<1>(results).value(), 43);
}