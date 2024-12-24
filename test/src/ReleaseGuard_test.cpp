#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/ReleaseGuard.hpp>

struct ReleaseGuardDeviceMock
{
    MOCK_METHOD(void, Release, ());
};

TEST(ReleaseGuardTest, ReleaseGuardTest_constructor)
{
    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);

    tinycoro::ReleaseGuard lock{mock};
}

TEST(ReleaseGuardTest, ReleaseGuardTest_move_constructor)
{
    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);
    
    tinycoro::ReleaseGuard lock1{mock};
    tinycoro::ReleaseGuard lock2{std::move(lock1)};
    
    // empty guard
    tinycoro::ReleaseGuard lock3{std::move(lock1)};
}

TEST(ReleaseGuardTest, ReleaseGuardTest_move_assign_operator)
{
    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);
    
    tinycoro::ReleaseGuard lock1{mock};
    tinycoro::ReleaseGuard lock2 = std::move(lock1);
    
    // empty gurad
    tinycoro::ReleaseGuard lock3 = std::move(lock1);
}

TEST(ReleaseGuardTest, ReleaseGuardTest_owns_lock)
{
    ReleaseGuardDeviceMock mock;

    EXPECT_CALL(mock, Release()).Times(1);
    
    tinycoro::ReleaseGuard lock1{mock};
    EXPECT_TRUE(lock1.owns_lock());

    tinycoro::ReleaseGuard lock2 = std::move(lock1);
    EXPECT_FALSE(lock1.owns_lock());
    EXPECT_TRUE(lock2.owns_lock());
    
    // empty gurad
    tinycoro::ReleaseGuard lock3 = std::move(lock1);
    EXPECT_FALSE(lock3.owns_lock());

    tinycoro::ReleaseGuard lock4{std::move(lock2)};
    EXPECT_FALSE(lock2.owns_lock());
    EXPECT_TRUE(lock4.owns_lock());
}