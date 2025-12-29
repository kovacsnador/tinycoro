#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/ReleaseGuard.hpp>

struct ReleaseGuardDeviceMock
{
    MOCK_METHOD(void, Release, ());
};

template<typename T>
struct ReleaseGuardTest : testing::Test
{
    using value_type = T;
};

using ReleaseGuardTest_Types = testing::Types<tinycoro::ReleaseGuard<ReleaseGuardDeviceMock>,
    tinycoro::ReleaseGuardRelaxed<ReleaseGuardDeviceMock>>;

TYPED_TEST_SUITE(ReleaseGuardTest, ReleaseGuardTest_Types);

TYPED_TEST(ReleaseGuardTest, ReleaseGuardTest_constructor)
{
    using ReleaseGuard_t = TestFixture::value_type;

    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);

    ReleaseGuard_t lock{mock};
}

TYPED_TEST(ReleaseGuardTest, ReleaseGuardTest_move_constructor)
{
    using ReleaseGuard_t = TestFixture::value_type;

    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);
    
    ReleaseGuard_t         lock1{mock};
    ReleaseGuard_t lock2{std::move(lock1)};
    
    // empty guard
    ReleaseGuard_t lock3{std::move(lock1)};
}

TYPED_TEST(ReleaseGuardTest, ReleaseGuardTest_move_assign_operator)
{
    using ReleaseGuard_t = TestFixture::value_type;

    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);
    
    ReleaseGuard_t         lock1{mock};
    ReleaseGuard_t lock2 = std::move(lock1);
    
    // empty gurad
    ReleaseGuard_t lock3 = std::move(lock1);
}

TYPED_TEST(ReleaseGuardTest, ReleaseGuardTest_owns_lock)
{
    using ReleaseGuard_t = TestFixture::value_type;

    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);
    
    ReleaseGuard_t lock1{mock};
    EXPECT_TRUE(lock1.owns_lock());

    ReleaseGuard_t lock2 = std::move(lock1);
    EXPECT_FALSE(lock1.owns_lock());
    EXPECT_TRUE(lock2.owns_lock());
    
    // empty gurad
    ReleaseGuard_t lock3 = std::move(lock1);
    EXPECT_FALSE(lock3.owns_lock());

    ReleaseGuard_t lock4{std::move(lock2)};
    EXPECT_FALSE(lock2.owns_lock());
    EXPECT_TRUE(lock4.owns_lock());
}