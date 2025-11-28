// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------
#ifndef TINY_CORO_Bit_LOCK_HPP
#define TINY_CORO_Bit_LOCK_HPP

#include <atomic>

namespace tinycoro { namespace detail {

    // Tag type indicating that a bit should be set to 1.
    struct BitSetTag {};

    // Tag type indicating that a bit should be cleared to 0.
    struct BitClearTag {};

    // Atomically sets or clears a specific bit in an atomic variable.
    //
    // StrategyT  – Either AtomicSet or AtomicUnset to indicate the operation.
    // T          – Underlying integral type of the atomic.
    // Bit        – Zero-based bit index to modify.
    // atom       – Atomic value whose Bit-th bit will be updated.
    //
    // This function performs a retry loop using compare_exchange_weak.
    // It only performs the atomic exchange when the target bit actually needs
    // to change, minimizing unnecessary atomic operations.
    //
    // This function can serve as the building block for lightweight atomic locks.
    template <typename StrategyT, typename T, auto Bit>
    constexpr void AtomicModifyBit(std::atomic<T>& atom) noexcept
    {
        auto mask = T{1} << Bit;
        for (;;)
        {
            auto old = atom.load(std::memory_order::relaxed);
            T    desired{};

            if constexpr (std::same_as<StrategyT, BitSetTag>)
                desired = old | mask;
            else
                desired = old & ~mask;

            if (old != desired)
            {
                // try to exchange
                if (atom.compare_exchange_weak(old, desired, std::memory_order::release, std::memory_order::relaxed))
                {
                    // we could flip the bit
                    return;
                }
            }
        }
    }

    // A tiny bit-based mutex implemented on top of an atomic integer.
    //
    // The Bit-th bit of the referenced atomic variable acts as the lock flag.
    // This avoids the storage overhead of a full std::mutex and allows multiple
    // independent locks to reside inside a single atomic variable if desired.
    //
    // Usage:
    //    BitLock<uint32_t, 3> lock(my_atomic);
    //    lock.lock();   // Sets bit 3
    //    lock.unlock(); // Clears bit 3
    template <typename T, auto Bit>
    struct BitLock
    {
        constexpr BitLock(std::atomic<T>& a)
        : _atom{a}
        {
        }

        constexpr void lock() noexcept { AtomicModifyBit<BitSetTag, T, Bit>(_atom); }

        constexpr void unlock() noexcept { AtomicModifyBit<BitClearTag, T, Bit>(_atom); }

    private:
        std::atomic<T>& _atom;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_Bit_LOCK_HPP