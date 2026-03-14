#include <gtest/gtest.h>

#include <tinycoro/Scheduler.hpp>

#include <utility>

namespace {
    struct FakeScheduler
    {
        void _Acquire() noexcept
        {
            ++acquireCount;
        }

        void _Release() noexcept
        {
            ++releaseCount;
        }

        int acquireCount{};
        int releaseCount{};
    };
} // namespace

TEST(WorkGuardTest, destructor_calls_release_once)
{
    int called = 0;
    {
        tinycoro::WorkGuard guard{[&called] { ++called; }};
        EXPECT_EQ(called, 0);
    }

    EXPECT_EQ(called, 1);
}

TEST(WorkGuardTest, unlock_calls_release_once)
{
    int called = 0;

    tinycoro::WorkGuard guard{[&called] { ++called; }};
    guard.Unlock();
    guard.Unlock();

    EXPECT_EQ(called, 1);
}

TEST(WorkGuardTest, move_constructor_transfers_ownership)
{
    int called = 0;

    tinycoro::WorkGuard movedTo;
    {
        tinycoro::WorkGuard original{[&called] { ++called; }};
        movedTo = std::move(original);
    }

    EXPECT_EQ(called, 0);
    movedTo.Unlock();
    EXPECT_EQ(called, 1);
}

TEST(WorkGuardTest, move_assignment_releases_current_and_transfers_new)
{
    int lhsCalled = 0;
    int rhsCalled = 0;

    tinycoro::WorkGuard lhs{[&lhsCalled] { ++lhsCalled; }};
    tinycoro::WorkGuard rhs{[&rhsCalled] { ++rhsCalled; }};

    lhs = std::move(rhs);

    EXPECT_EQ(lhsCalled, 1);
    EXPECT_EQ(rhsCalled, 0);

    lhs.Unlock();
    EXPECT_EQ(rhsCalled, 1);
}

TEST(WorkGuardTest, swap_exchanges_callbacks)
{
    int aCalled = 0;
    int bCalled = 0;

    tinycoro::WorkGuard a{[&aCalled] { ++aCalled; }};
    tinycoro::WorkGuard b{[&bCalled] { ++bCalled; }};

    a.swap(b);

    a.Unlock();
    b.Unlock();

    EXPECT_EQ(aCalled, 1);
    EXPECT_EQ(bCalled, 1);
}

TEST(WorkGuardTest, make_work_guard_acquire_and_release)
{
    FakeScheduler scheduler{};

    {
        auto guard = tinycoro::MakeWorkGuard(scheduler);
        EXPECT_EQ(scheduler.acquireCount, 1);
        EXPECT_EQ(scheduler.releaseCount, 0);
    }

    EXPECT_EQ(scheduler.acquireCount, 1);
    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, make_work_guard_unlock_releases_once)
{
    FakeScheduler scheduler{};

    auto guard = tinycoro::MakeWorkGuard(scheduler);
    EXPECT_EQ(scheduler.acquireCount, 1);
    EXPECT_EQ(scheduler.releaseCount, 0);

    guard.Unlock();
    guard.Unlock();

    EXPECT_EQ(scheduler.releaseCount, 1);
}
