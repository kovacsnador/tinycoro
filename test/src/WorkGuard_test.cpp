#include <gtest/gtest.h>

#include <tinycoro/Scheduler.hpp>

#include <utility>
#include <thread>
#include <vector>
#include <atomic>

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
    FakeScheduler scheduler;
    {
        tinycoro::WorkGuard guard{scheduler};
        EXPECT_EQ(scheduler.acquireCount, 1);
    }

    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, self_move_assign)
{
    FakeScheduler scheduler;
    {
        tinycoro::WorkGuard guard{scheduler};

        // to avoid warning
        auto ptr = std::addressof(guard);
        guard = std::move(*ptr);

        EXPECT_EQ(scheduler.releaseCount, 0);
    }

    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, self_swap)
{
    FakeScheduler scheduler;
    {
        tinycoro::WorkGuard guard{scheduler};
        guard.swap(guard);

        EXPECT_EQ(scheduler.releaseCount, 0);
    }

    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, unlock_calls_release_once)
{
    FakeScheduler scheduler;

    tinycoro::WorkGuard guard{scheduler};
    guard.Unlock();
    guard.Unlock();

    EXPECT_EQ(scheduler.acquireCount, 1);
    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, unlock_is_thread_safe)
{
    FakeScheduler scheduler;

    tinycoro::WorkGuard guard{scheduler};

    const int numThreads = 10;
    std::vector<std::thread> threads;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&guard]() {
            guard.Unlock();
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    EXPECT_EQ(scheduler.acquireCount, 1);
    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, move_constructor_transfers_ownership)
{
    FakeScheduler scheduler;

    tinycoro::WorkGuard<FakeScheduler> movedTo;
    {
        tinycoro::WorkGuard original{scheduler};
        movedTo = std::move(original);
    }

    EXPECT_EQ(scheduler.releaseCount, 0);
    movedTo.Unlock();
    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, move_assignment_releases_current_and_transfers_new)
{
    FakeScheduler scheduler1;
    FakeScheduler scheduler2;

    tinycoro::WorkGuard lhs{scheduler1};
    tinycoro::WorkGuard rhs{scheduler2};

    lhs = std::move(rhs);

    EXPECT_EQ(scheduler1.releaseCount, 1);
    EXPECT_EQ(scheduler2.releaseCount, 0);

    lhs.Unlock();
    EXPECT_EQ(scheduler2.releaseCount, 1);
}

TEST(WorkGuardTest, swap_exchanges_callbacks)
{
    FakeScheduler scheduler1;
    FakeScheduler scheduler2;

    tinycoro::WorkGuard a{scheduler1};
    tinycoro::WorkGuard b{scheduler2};

    a.swap(b);

    EXPECT_EQ(scheduler1.releaseCount, 0);
    EXPECT_EQ(scheduler2.releaseCount, 0);

    a.Unlock();
    
    EXPECT_EQ(scheduler1.releaseCount, 0);
    EXPECT_EQ(scheduler2.releaseCount, 1);

    b.Unlock();

    EXPECT_EQ(scheduler1.releaseCount, 1);
    EXPECT_EQ(scheduler2.releaseCount, 1);
}

TEST(WorkGuardTest, make_work_guard_acquire_and_release)
{
    FakeScheduler scheduler{};

    {
        tinycoro::WorkGuard guard{scheduler};

        EXPECT_EQ(scheduler.acquireCount, 1);
        EXPECT_EQ(scheduler.releaseCount, 0);
    }

    EXPECT_EQ(scheduler.acquireCount, 1);
    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, make_work_guard_unlock_releases_once)
{
    FakeScheduler scheduler{};

    tinycoro::WorkGuard guard{scheduler};

    EXPECT_EQ(scheduler.acquireCount, 1);
    EXPECT_EQ(scheduler.releaseCount, 0);

    guard.Unlock();
    guard.Unlock();

    EXPECT_EQ(scheduler.releaseCount, 1);
}

TEST(WorkGuardTest, multithreaded_test)
{
    FakeScheduler scheduler1{};
    FakeScheduler scheduler2{};
    FakeScheduler scheduler3{};

    using WorkGuard_t = tinycoro::WorkGuard<FakeScheduler>;

    std::vector<WorkGuard_t> guards1(1000);
    std::vector<WorkGuard_t> guards2(1000);
    std::vector<WorkGuard_t> guards3(1000);

    for(size_t i = 0; i < 1000; ++i)
    {
        guards1[i] = WorkGuard_t{scheduler1};
        guards2[i] = WorkGuard_t{scheduler2};
        guards3[i] = WorkGuard_t{scheduler3};
    }

    std::vector<std::future<void>> futures;

    for(size_t i = 0; i < 10; ++i)
    {
        futures.emplace_back(std::async(std::launch::async, [&]{
            for(size_t j=0; j < 1000; ++j)
            {
                guards1[j] = std::move(guards2[j]);
                guards3[j] = std::move(guards1[j]);
                guards2[j] = std::move(guards3[j]);
            }
        }));
    }

    futures.clear();

    guards1.clear();
    guards2.clear();
    guards3.clear();

    EXPECT_EQ(scheduler1.acquireCount, 1000);
    EXPECT_EQ(scheduler1.releaseCount, 1000);

    EXPECT_EQ(scheduler2.acquireCount, 1000);
    EXPECT_EQ(scheduler2.releaseCount, 1000);

    EXPECT_EQ(scheduler3.acquireCount, 1000);
    EXPECT_EQ(scheduler3.releaseCount, 1000);
}
