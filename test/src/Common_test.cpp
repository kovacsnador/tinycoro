#include <gtest/gtest.h>

#include <vector>
#include <list>
#include <array>
#include <map>
#include <type_traits>
#include <concepts>
#include <tuple>
#include <coroutine>

#include <tinycoro/Common.hpp>
#include <tinycoro/LinkedPtrStack.hpp>
#include <tinycoro/LinkedUtils.hpp>
#include <tinycoro/AllocatorAdapter.hpp>

template <typename T>
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

    using first_t    = std::tuple_element<0, T>::type;
    using expected_t = std::tuple_element<1, T>::type;

    EXPECT_EQ(tinycoro::concepts::Iterable<first_t>, expected_t::value);
}

struct NoVirtualDestructor
{
};

struct VirtualDestructor1
{
    virtual ~VirtualDestructor1() { }
};

struct VirtualDestructor2
{
    virtual ~VirtualDestructor2() = 0;
};

struct NoVirtualDestructorAbstract
{
    virtual void foo() = 0;
};

template <typename T>
struct Concepts_HasVirtualDestructor : public testing::Test
{
    using value_type = T;
};

using HasVirtualDestructor_types = testing::
    Types<std::tuple<std::string, std::false_type>, std::tuple<NoVirtualDestructor, std::false_type>, std::tuple<VirtualDestructor1, std::true_type>>;

TYPED_TEST_SUITE(Concepts_HasVirtualDestructor, HasVirtualDestructor_types);

TYPED_TEST(Concepts_HasVirtualDestructor, Concepts_HasVirtualDestructor)
{
    using T = typename TestFixture::value_type;

    using first_t    = std::tuple_element<0, T>::type;
    using expected_t = std::tuple_element<1, T>::type;

    EXPECT_EQ(tinycoro::concepts::HasVirtualDestructor<first_t>, expected_t::value);
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

TEST(Helper_ContainsTest, Helper_ContainsTest)
{
    struct Node : tinycoro::detail::SingleLinkable<Node>
    {
    };

    Node n1{};
    Node n2{};
    Node n3{};
    Node n4{};

    Node n5{};

    tinycoro::detail::LinkedPtrStack<Node> list;

    list.push(&n1);
    list.push(&n2);
    list.push(&n3);
    list.push(&n4);

    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n1));
    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n2));
    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n3));
    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n4));

    EXPECT_FALSE(tinycoro::detail::helper::Contains(list.top(), &n5));

    list.erase(&n2);

    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n1));
    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n3));
    EXPECT_TRUE(tinycoro::detail::helper::Contains(list.top(), &n4));

    EXPECT_FALSE(tinycoro::detail::helper::Contains(list.top(), &n5));
    EXPECT_FALSE(tinycoro::detail::helper::Contains(list.top(), &n2));
}

template <typename>
struct EmptyClass
{
};

template <typename PromiseT>
struct AllocatorAdapterNoexcept
{
    // ensure the use of non-throwing operator-new
    [[noreturn]] static std::coroutine_handle<PromiseT> get_return_object_on_allocation_failure() { throw std::bad_alloc{}; }

    [[nodiscard]] static void* operator new(size_t nbytes) noexcept { return std::malloc(nbytes); }

    static void operator delete(void* ptr, [[maybe_unused]] size_t nbytes) noexcept { std::free(ptr); }
};

template <typename>
struct AllocatorAdapterExcept
{
    [[nodiscard]] static void* operator new(size_t nbytes) { return std::malloc(nbytes); }

    static void operator delete(void* ptr, [[maybe_unused]] size_t nbytes) noexcept { std::free(ptr); }
};

template <typename T>
struct Concepts_AllocatorAdapter : testing::Test
{
    using value_type = T;
};

using AllocatorAdapterTyped = testing::Types<std::tuple<EmptyClass<int>, std::true_type>,
                                             std::tuple<tinycoro::DefaultAllocator<int>, std::true_type>,
                                             std::tuple<AllocatorAdapterNoexcept<int>, std::true_type>,
                                             std::tuple<AllocatorAdapterExcept<int>, std::true_type>,
                                             std::tuple<std::string, std::false_type>,
                                             std::tuple<std::list<int>, std::false_type>>;

TYPED_TEST_SUITE(Concepts_AllocatorAdapter, AllocatorAdapterTyped);

TYPED_TEST(Concepts_AllocatorAdapter, Concepts_AllocatorAdapter)
{
    using tuple_t = typename TestFixture::value_type;

    using first_t    = std::tuple_element<0, tuple_t>::type;
    using expected_t = std::tuple_element<1, tuple_t>::type;

    EXPECT_EQ(tinycoro::concepts::IsAllocatorAdapterT<first_t>, expected_t::value);
}
