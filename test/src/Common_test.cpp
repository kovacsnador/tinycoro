#include <gtest/gtest.h>

#include <vector>
#include <list>
#include <array>
#include <map>
#include <type_traits>
#include <concepts>
#include <tuple>

#include <tinycoro/Common.hpp>

template<typename T>
struct Concepts_IterableTest : public testing::Test
{
    using value_type = T;
};

using IterableTypes = testing::Types<std::tuple<std::vector<int>, std::true_type>,
                                         std::tuple<std::list<int>, std::true_type>,
                                          std::tuple<std::array<int, 10>, std::true_type>,
                                           std::tuple<std::map<int, int>, std::true_type>,
                                           std::tuple<int, std::false_type>,
                                           std::tuple<float, std::false_type>,
                                           std::tuple<std::monostate, std::false_type>>;

TYPED_TEST_SUITE(Concepts_IterableTest, IterableTypes);

TYPED_TEST(Concepts_IterableTest, Concepts_IterableTest)
{
    using T = typename TestFixture::value_type;

    using firstParamT = std::decay_t<decltype(std::get<0>(std::declval<T>()))>;
    using secondParamT = std::decay_t<decltype(std::get<1>(std::declval<T>()))>;

    if constexpr (std::same_as<secondParamT, std::true_type>)
    {
        if constexpr (!tinycoro::concepts::Iterable<firstParamT>) {
            EXPECT_FALSE(true)  << "Type not iterable!";   
        }
    }
    else
    {
        if constexpr (tinycoro::concepts::Iterable<firstParamT>) {
            EXPECT_FALSE(true)  << "Type is iterable but should be not!";  
        }
    }
}

struct NoVirtualDestructor
{
};

struct VirtualDestructor1
{
    virtual ~VirtualDestructor1() {}
};

struct VirtualDestructor2
{
    virtual ~VirtualDestructor2() = 0;
};

struct NoVirtualDestructorAbstract
{
    virtual void foo() = 0;
};


template<typename T>
struct Concepts_HasVirtualDestructor : public testing::Test
{
    using value_type = T;
};

using HasVirtualDestructor_types = testing::Types<std::tuple<std::string, std::false_type>,
                                                    std::tuple<NoVirtualDestructor, std::false_type>,
                                                    std::tuple<VirtualDestructor1, std::true_type>>;

TYPED_TEST_SUITE(Concepts_HasVirtualDestructor, HasVirtualDestructor_types);

TYPED_TEST(Concepts_HasVirtualDestructor, Concepts_HasVirtualDestructor)
{
    using T = typename TestFixture::value_type;

    using firstParamT = std::decay_t<decltype(std::get<0>(std::declval<T>()))>;
    using secondParamT = std::decay_t<decltype(std::get<1>(std::declval<T>()))>;

    if constexpr (std::same_as<secondParamT, std::true_type>)
    {
        EXPECT_TRUE((tinycoro::concepts::HasVirtualDestructor<firstParamT>)) << "Type has no virtual destructor!";
    }
    else
    {
        EXPECT_FALSE((tinycoro::concepts::HasVirtualDestructor<firstParamT>)) << "Type has virtual destructor!";
    }
}

TEST(Concepts_HasVirtualDestructor, Concepts_HasVirtualDestructor_abstractClass_true)
{
    EXPECT_TRUE((tinycoro::concepts::HasVirtualDestructor<VirtualDestructor2>)) << "Type has no virtual destructor!";
}

TEST(Concepts_HasVirtualDestructor, Concepts_HasVirtualDestructor_abstractClass_false)
{
    EXPECT_FALSE((tinycoro::concepts::HasVirtualDestructor<NoVirtualDestructorAbstract>)) << "Type has virtual destructor!";
}

TEST(Helper_AutoResetEvent, Helper_AutoResetEvent_defaultConstructor)
{
    tinycoro::detail::helper::AutoResetEvent event;
    EXPECT_FALSE(event.IsSet());

    event.Set();

    EXPECT_TRUE(event.IsSet());
    EXPECT_TRUE(event.Wait());

    EXPECT_FALSE(event.IsSet());
}

TEST(Helper_AutoResetEvent, Helper_AutoResetEvent_customConstructor)
{
    tinycoro::detail::helper::AutoResetEvent event{true};
    EXPECT_TRUE(event.IsSet());

    EXPECT_TRUE(event.Wait());
    EXPECT_FALSE(event.IsSet());

    event.Set();

    EXPECT_TRUE(event.IsSet());
}
