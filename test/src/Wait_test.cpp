#include <gtest/gtest.h>

#include <future>

#include <tinycoro/Future.hpp>
#include <tinycoro/Wait.hpp>

template <typename T>
class TD;

template <typename T>
struct GetAllTest : testing::Test
{
    using value_type = T;
};

using GetAllTestTypes = testing::Types<std::promise<int32_t>, tinycoro::FutureState<int32_t>, std::promise<void>, tinycoro::FutureState<void>>;

TYPED_TEST_SUITE(GetAllTest, GetAllTestTypes);

TYPED_TEST(GetAllTest, GetAllTest_one_Future)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT promise;
    auto     future = promise.get_future();

    if constexpr (std::same_as<int32_t, ValueT>)
    {
        promise.set_value(42);
        auto value = tinycoro::GetAll(future);
        EXPECT_EQ(value, 42);
    }
    else
    {
        promise.set_value();
        EXPECT_NO_THROW(tinycoro::GetAll(future));
    }
}

TYPED_TEST(GetAllTest, GetAllTest_one_Future_exception)
{
    using PromiseT = TestFixture::value_type;
    using ValueT   = std::decay_t<decltype(std::declval<PromiseT>().get_future().get())>;

    PromiseT promise;
    auto     future = promise.get_future();

    if constexpr (std::same_as<int32_t, ValueT>)
    {
        promise.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));

        auto getAll = [&future] { std::ignore = tinycoro::GetAll(future); };
        EXPECT_THROW(getAll(), std::runtime_error);
    }
    else
    {
        promise.set_exception(std::make_exception_ptr(std::runtime_error{"error"}));
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

    if constexpr (std::same_as<int32_t, ValueT>)
    {
        p1.set_value(40);
        p2.set_value(41);
        p3.set_value(42);

        auto results = tinycoro::GetAll(tuple);

        EXPECT_EQ(std::get<0>(results), 40);
        EXPECT_EQ(std::get<1>(results), 41);
        EXPECT_EQ(std::get<2>(results), 42);
    }
    else
    {
        p1.set_value();
        p2.set_value();
        p3.set_value();

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

    if constexpr (std::same_as<int32_t, ValueT>)
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
        p2.set_value();
        p3.set_value();

        EXPECT_THROW(tinycoro::GetAll(tuple), std::runtime_error);
    }
}

template <typename T>
struct GetAllTestWithTupleMixed : testing::Test
{
    using value_type = T;
};

using GetAllTestWithTupleMixedTypes = testing::Types<std::promise<int32_t>, std::promise<void>>;

TYPED_TEST_SUITE(GetAllTestWithTupleMixed, GetAllTestWithTupleMixedTypes);

template <typename T>
struct GetAllTestWithTupleMixedFutureState : testing::Test
{
    using value_type = T;
};

using GetAllTestWithTupleMixedFutureStateTypes = testing::Types<tinycoro::FutureState<int32_t>, tinycoro::FutureState<void>>;

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

    if constexpr (std::same_as<int32_t, ValueT>)
    {
        p1.set_value(40);
        p2.set_value(41);
        p3.set_value(42);
    }
    else
    {
        p1.set_value();
        p2.set_value();
        p3.set_value();
    }

    p4.set_value(S{true, 42});
    p5.set_value(3.14f);

    auto results = tinycoro::GetAll(tuple);

    if constexpr (std::same_as<int32_t, ValueT>)
    {
        EXPECT_EQ(std::get<0>(results), 40);
        EXPECT_EQ(std::get<1>(results), 41);
        EXPECT_EQ(std::get<2>(results), 42);
    }
    else
    {
        EXPECT_TRUE((std::same_as<tinycoro::VoidType, std::decay_t<decltype(std::get<0>(results))>>));
        EXPECT_TRUE((std::same_as<tinycoro::VoidType, std::decay_t<decltype(std::get<1>(results))>>));
        EXPECT_TRUE((std::same_as<tinycoro::VoidType, std::decay_t<decltype(std::get<2>(results))>>));
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

    GetAllTestWithTupleMixedTestHelper<tinycoro::FutureState, PromiseT, ValueT>();
}