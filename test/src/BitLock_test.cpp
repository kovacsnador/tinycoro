#include <gtest/gtest.h>

#include <thread>
#include <ranges>
#include <mutex>

#include "tinycoro/BitLock.hpp"

TEST(AtomicModifyBitTest, AtomicModifyBitTest_set)
{
    std::atomic<uint32_t> atom{};
    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 0u>(atom);
    EXPECT_EQ(atom.load(), 1u);
}

TEST(AtomicModifyBitTest, AtomicModifyBitTest_unset)
{
    std::atomic<uint32_t> atom{1u};
    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 0u>(atom);
    EXPECT_EQ(atom.load(), 0);
}

TEST(AtomicModifyBitTest, AtomicModifyBitTest_set_unset)
{
    std::atomic<uint32_t> atom{0u};
    
    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 0u>(atom);
    EXPECT_EQ(atom.load(), 1u);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 0u>(atom);
    EXPECT_EQ(atom.load(), 0);
}


TEST(AtomicModifyBitTest, AtomicModifyBitTest_clear_bits)
{
    std::atomic<uint32_t> atom{0b1111'1111}; // bit 3 already set

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 7>(atom);
    EXPECT_EQ(atom.load(), 0b0111'1111);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 6>(atom);
    EXPECT_EQ(atom.load(), 0b0011'1111);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 5>(atom);
    EXPECT_EQ(atom.load(), 0b0001'1111);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 4>(atom);
    EXPECT_EQ(atom.load(), 0b0000'1111);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 3>(atom);
    EXPECT_EQ(atom.load(), 0b0000'0111);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 2>(atom);
    EXPECT_EQ(atom.load(), 0b0000'0011);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 1>(atom);
    EXPECT_EQ(atom.load(), 0b0000'0001);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitClearTag, uint32_t, 0>(atom);
    EXPECT_EQ(atom.load(), 0b0000'0000);
}

TEST(AtomicModifyBitTest, AtomicModifyBitTest_set_bits)
{
    std::atomic<uint32_t> atom{0b0000'0000}; // bit 3 already set

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 7>(atom);
    EXPECT_EQ(atom.load(), 0b1000'0000);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 6>(atom);
    EXPECT_EQ(atom.load(), 0b1100'0000);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 5>(atom);
    EXPECT_EQ(atom.load(), 0b1110'0000);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 4>(atom);
    EXPECT_EQ(atom.load(), 0b1111'0000);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 3>(atom);
    EXPECT_EQ(atom.load(), 0b1111'1000);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 2>(atom);
    EXPECT_EQ(atom.load(), 0b1111'1100);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 1>(atom);
    EXPECT_EQ(atom.load(), 0b1111'1110);

    tinycoro::detail::AtomicModifyBit<tinycoro::detail::BitSetTag, uint32_t, 0>(atom);
    EXPECT_EQ(atom.load(), 0b1111'1111);
}

TEST(BitLockTest, BitLockTest_lock)
{
    std::atomic<uint32_t> flags{0};
    tinycoro::detail::BitLock<uint32_t, 1> lock(flags);

    lock.lock();
    EXPECT_EQ(flags.load(), 1u << 1);
}

TEST(BitLockTest, BitLockTest_unlock)
{
    std::atomic<uint32_t> flags{1u << 1};
    tinycoro::detail::BitLock<uint32_t, 1> lock(flags);

    lock.unlock();
    EXPECT_EQ(flags.load(), 0);
}

TEST(BitLockTest, BitLockTest_no_conflict)
{
    std::atomic<uint32_t> flags{0};

    tinycoro::detail::BitLock<uint32_t, 0> lockA(flags);
    tinycoro::detail::BitLock<uint32_t, 5> lockB(flags);

    lockA.lock();
    EXPECT_EQ(flags.load(), 1u << 0);

    lockB.lock();
    EXPECT_EQ(flags.load(), (1u << 0) | (1u << 5));

    lockA.unlock();
    EXPECT_EQ(flags.load(), 1u << 5);

    lockB.unlock();
    EXPECT_EQ(flags.load(), 0u);
}

struct BitLockTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BitLockTest, BitLockTest, testing::Values(1, 10, 100, 1000, 10000, 100000));

TEST_P(BitLockTest, BitLockTest_concurrent)
{
    std::atomic<uint32_t> atom;
    tinycoro::detail::BitLock<uint32_t, 0u> mtx{atom};

    std::vector<std::jthread> threads;

    size_t count{};

    auto worker = [&] {
        for(size_t i = 0; i < GetParam(); ++i)
        {
            std::scoped_lock lock{mtx};
            count++;
        }
    };

    for(size_t i = 0; i < 8; ++i)
    {
        threads.emplace_back(worker);
    }

    std::for_each(threads.begin(), threads.end(), [](auto& t) { t.join(); });

    EXPECT_EQ(count, threads.size() * GetParam());
}