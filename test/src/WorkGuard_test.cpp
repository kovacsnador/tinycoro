#include <gtest/gtest.h>

#include <tinycoro/Scheduler.hpp>

#include <utility>
#include <thread>
#include <vector>
#include <atomic>
#include <latch>

namespace {
    struct FakeScheduler
    {
        void _Acquire() noexcept
        {
            acquireCount.fetch_add(1, std::memory_order::relaxed);
        }

        void _Release() noexcept
        {
            releaseCount.fetch_add(1, std::memory_order::relaxed);
        }

        std::atomic<int> acquireCount{};
        std::atomic<int> releaseCount{};
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

TEST(WorkGuardTest, default_constructed_guard_has_no_owner)
{
    tinycoro::WorkGuard<FakeScheduler> guard;

    EXPECT_FALSE(guard.Owner());
}

TEST(WorkGuardTest, constructed_guard_reports_owner_until_unlocked)
{
    FakeScheduler scheduler;

    tinycoro::WorkGuard guard{scheduler};
    EXPECT_TRUE(guard.Owner());

    EXPECT_TRUE(guard.Unlock());
    EXPECT_FALSE(guard.Owner());
}

TEST(WorkGuardTest, constructed_guard_copy_assign)
{
    FakeScheduler scheduler;

    {
        tinycoro::WorkGuard guard1{scheduler};
        tinycoro::WorkGuard guard2{scheduler};

        EXPECT_TRUE(guard1.Owner());
        EXPECT_TRUE(guard2.Owner());

        auto temp = guard1;
        guard1 = guard2;

        EXPECT_EQ(scheduler.releaseCount, 1);
        EXPECT_EQ(scheduler.acquireCount, 4);
    }

    EXPECT_EQ(scheduler.releaseCount, 4);
    EXPECT_EQ(scheduler.acquireCount, 4);
}

TEST(WorkGuardTest, copy_constructor_acquires_additional_ownership)
{
    FakeScheduler scheduler;

    {
        tinycoro::WorkGuard original{scheduler};
        tinycoro::WorkGuard copy{original};

        EXPECT_TRUE(original.Owner());
        EXPECT_TRUE(copy.Owner());
        EXPECT_EQ(scheduler.acquireCount, 2);
        EXPECT_EQ(scheduler.releaseCount, 0);
    }

    EXPECT_EQ(scheduler.acquireCount, 2);
    EXPECT_EQ(scheduler.releaseCount, 2);
}

TEST(WorkGuardTest, copy_assignment_releases_current_and_acquires_new_ownership)
{
    FakeScheduler scheduler1;
    FakeScheduler scheduler2;

    tinycoro::WorkGuard lhs{scheduler1};
    tinycoro::WorkGuard rhs{scheduler2};

    lhs = rhs;

    EXPECT_FALSE(lhs.Owner() == false);
    EXPECT_TRUE(rhs.Owner());
    EXPECT_EQ(scheduler1.acquireCount, 1);
    EXPECT_EQ(scheduler1.releaseCount, 1);
    EXPECT_EQ(scheduler2.acquireCount, 2);
    EXPECT_EQ(scheduler2.releaseCount, 0);

    EXPECT_TRUE(lhs.Unlock());
    EXPECT_TRUE(rhs.Unlock());
    EXPECT_EQ(scheduler2.releaseCount, 2);
}

TEST(WorkGuardTest, copying_empty_guard_keeps_target_empty)
{
    FakeScheduler scheduler;

    tinycoro::WorkGuard<FakeScheduler> empty;
    tinycoro::WorkGuard guard{scheduler};

    guard = empty;

    EXPECT_FALSE(guard.Owner());
    EXPECT_FALSE(empty.Owner());
    EXPECT_EQ(scheduler.acquireCount, 1);
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

TEST(WorkGuardTest, multi_swap)
{
    FakeScheduler scheduler;
    {
        tinycoro::WorkGuard guard1{scheduler};
        tinycoro::WorkGuard guard2{scheduler};

        for(size_t i = 0; i < 10000; ++i)
        {
            guard1.swap(guard2);
            guard2.swap(guard1);
            guard1.swap(guard2);
            guard2.swap(guard1);
        }

        EXPECT_EQ(scheduler.acquireCount, 2);
        EXPECT_EQ(scheduler.releaseCount, 0);
    }

    EXPECT_EQ(scheduler.releaseCount, 2);
    EXPECT_EQ(scheduler.acquireCount, 2);
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

TEST(WorkGuardTest, multithreaded_test_move)
{
    constexpr size_t count{100000};

    FakeScheduler scheduler1{};
    FakeScheduler scheduler2{};
    FakeScheduler scheduler3{};

    {
        using WorkGuard_t = tinycoro::WorkGuard<FakeScheduler>;

        std::vector<WorkGuard_t> guards1(count);
        std::vector<WorkGuard_t> guards2(count);
        std::vector<WorkGuard_t> guards3(count);

        std::latch latch{1};

        for(size_t i = 0; i < count; ++i)
        {
            guards1[i] = WorkGuard_t{scheduler1};
            guards2[i] = WorkGuard_t{scheduler2};
            guards3[i] = WorkGuard_t{scheduler3};
        }

        std::vector<std::future<void>> futures;

        for(size_t i = 0; i < 10; ++i)
        {
            futures.emplace_back(std::async(std::launch::async, [&]{

                latch.wait();

                for(size_t j = 0; j < count; ++j)
                {
                    WorkGuard_t temp = std::move(guards1[j]);
                    guards1[j] = std::move(guards2[j]);
                    guards2[j] = std::move(guards3[j]);
                    guards3[j] = std::move(temp);
                }
            }));
        }

        latch.count_down();

        for(auto& it : futures)
            it.wait();

        EXPECT_EQ(scheduler1.acquireCount, count);
        EXPECT_EQ(scheduler2.acquireCount, count);
        EXPECT_EQ(scheduler3.acquireCount, count);
    }

    EXPECT_EQ(scheduler1.acquireCount, count);
    EXPECT_EQ(scheduler1.releaseCount, count);

    EXPECT_EQ(scheduler2.acquireCount, count);
    EXPECT_EQ(scheduler2.releaseCount, count);

    EXPECT_EQ(scheduler3.acquireCount, count);
    EXPECT_EQ(scheduler3.releaseCount, count);
}

TEST(WorkGuardTest, multithreaded_test_copy)
{
    constexpr size_t count{100000};

    FakeScheduler scheduler1{};
    FakeScheduler scheduler2{};
    FakeScheduler scheduler3{};

    {
        using WorkGuard_t = tinycoro::WorkGuard<FakeScheduler>;

        std::vector<WorkGuard_t> guards1(count);
        std::vector<WorkGuard_t> guards2(count);
        std::vector<WorkGuard_t> guards3(count);

        std::latch latch{1};

        for(size_t i = 0; i < count; ++i)
        {
            guards1[i] = WorkGuard_t{scheduler1};
            guards2[i] = WorkGuard_t{scheduler2};
            guards3[i] = WorkGuard_t{scheduler3};
        }

        std::vector<std::future<void>> futures;

        for(size_t i = 0; i < 10; ++i)
        {
            futures.emplace_back(std::async(std::launch::async, [&]{

                latch.wait();

                for(size_t j = 0; j < count; ++j)
                {
                    WorkGuard_t temp = guards1[j];
                    guards1[j] = guards2[j];
                    guards2[j] = guards3[j];
                    guards3[j] = temp;
                }
            }));
        }

        latch.count_down();

        for(auto& it : futures)
            it.wait();

    }

    EXPECT_EQ(scheduler1.acquireCount, scheduler1.releaseCount);
    EXPECT_EQ(scheduler2.acquireCount, scheduler2.releaseCount);
    EXPECT_EQ(scheduler3.acquireCount, scheduler3.releaseCount);
}