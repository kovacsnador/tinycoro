#include <gtest/gtest.h>

#include <string>
#include <variant>

#include <tinycoro/PtrVisitor.hpp>

TEST(PtrVisitorTest, PtrVisitorTest_non_const_variant)
{
    std::string str{"hello"};

    std::variant<std::string*, std::string> var;

    var = str;

    auto v = tinycoro::detail::PtrVisit(var);
    EXPECT_TRUE((std::same_as<decltype(v), std::string*>));
    EXPECT_EQ(*v, str);
}

TEST(PtrVisitorTest, PtrVisitorTest_const_variant)
{
    std::string str{"hello"};

    const std::variant<std::string*, std::string> var{str};

    auto v = tinycoro::detail::PtrVisit(var);
    EXPECT_TRUE((std::same_as<decltype(v), const std::string*>));
    EXPECT_EQ(*v, str);
}

TEST(PtrVisitorTest, PtrVisitorTest_non_const_variant2)
{
    std::variant<int, int*> var{42};

    auto v = tinycoro::detail::PtrVisit(var);
    EXPECT_TRUE((std::same_as<decltype(v), int*>));
    EXPECT_EQ(*v, 42);

    int i{44};
    var = &i;

    auto v2 = tinycoro::detail::PtrVisit(var);
    EXPECT_TRUE((std::same_as<decltype(v2), decltype(v)>));
    EXPECT_EQ(*v2, 44);
}

TEST(PtrVisitorTest, PtrVisitorTest_const_variant2)
{
    const std::variant<int, int*> var{42};

    auto v = tinycoro::detail::PtrVisit(var);
    EXPECT_TRUE((std::same_as<decltype(v), const int*>));
    EXPECT_EQ(*v, 42);


    int i{44};
    const std::variant<int, int*> var2{&i};

    auto v2 = tinycoro::detail::PtrVisit(var2);
    EXPECT_TRUE((std::same_as<decltype(v2), const int*>));
    EXPECT_TRUE((std::same_as<decltype(v2), decltype(v)>));
    EXPECT_EQ(*v2, 44);
}

template<typename T>
struct PtrVisitorTypedTest : testing::Test
{
    using value_type = T;
};

using PtrVisitorTypes = testing::Types<std::tuple<std::variant<int>, std::true_type>,
                                       std::tuple<std::variant<int*, int>, std::true_type>,
                                       std::tuple<std::variant<int, int*>, std::true_type>,
                                       std::tuple<std::variant<int, std::string>, std::false_type>,
                                       std::tuple<std::variant<int*, double*>, std::false_type>,
                                       std::tuple<std::variant<int*, int*, int>, std::true_type>,
                                       std::tuple<std::variant<const int*, int*, int>, std::true_type>,
                                       std::tuple<const std::variant<int*, int>, std::false_type>>;

TYPED_TEST_SUITE(PtrVisitorTypedTest, PtrVisitorTypes);

TYPED_TEST(PtrVisitorTypedTest, PtrVisitorTest_validate_variants)
{
    using tuple_t = typename TestFixture::value_type;

    using first_t = std::tuple_element_t<0, tuple_t>;
    using second_t = std::tuple_element_t<1, tuple_t>;

    EXPECT_TRUE((tinycoro::detail::local::ValidatePtrVariant_v<first_t> == second_t::value));
}