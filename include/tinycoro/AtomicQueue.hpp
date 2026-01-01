// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_ATOMIC_QUEUE_HPP
#define TINY_CORO_ATOMIC_QUEUE_HPP

#include <concepts>
#include <array>
#include <type_traits>

#include "CachelineAlign.hpp"
#include "Common.hpp"

namespace tinycoro { namespace detail {

    // A multi-producer, multi-consumer (MPMC) lock-free queue.
    //
    // Internally, this queue uses a fixed-size ring buffer of size `Capacity`,
    // which **must be a power of two**. Indexing is done via masking for efficiency.
    //
    // The queue supports concurrent `try_push` and `try_pop` operations
    // without using mutexes. All synchronization is done via atomics and
    // carefully chosen memory orderings.
    //
    // Design overview:
    // - `_head` and `_tail` are monotonically increasing sequence counters
    //   that represent global push and pop positions.
    // - Each buffer slot (`Element`) has its own `sequence` counter that
    //   encodes the state of that slot across wraparounds.
    // - The combination of `(position / Capacity)` (the “turn”) and the
    //   per-element sequence number ensures correct coordination between
    //   producers and consumers, even when indices wrap around the ring.
    //
    // Sequence counters:
    // - `_head`, `_tail`, and `Element::sequence` are 64-bit unsigned integers
    //   (`uint64_t`).
    // - Sequence values are monotonically increasing and are **not wrapped
    //   or masked**.
    //
    // ⚠ Overflow note:
    // If any sequence counter overflows (after 2^64 operations), correctness
    // is no longer guaranteed. In practice, this would require an unrealistically
    // high number of operations (even at billions of ops/sec, hundreds of years),
    // so this is considered a theoretical limitation.
    //
    // ⚠ Lock-free guarantee:
    // The queue is lock-free only if `std::atomic<uint64_t>` is lock-free on
    // the target platform.
    // You can check this via `std::atomic<uint64_t>::is_always_lock_free`.
    // On most modern 64-bit platforms, this condition holds. If not, the
    // implementation may internally fall back to locks or syscalls.
    //
    // Properties:
    // - Multiple producers and multiple consumers
    // - Lock-free (subject to atomic guarantees)
    // - Bounded capacity (`Capacity`)
    // - Non-blocking API (`try_push`, `try_pop`)
    //
    // This queue is well suited for high-throughput, low-latency systems
    // where bounded capacity and platform atomic guarantees are acceptable.
    template <typename T, uint64_t Capacity>
        requires detail::IsPowerOf2<Capacity>::value
    class AtomicQueue
    {
    public:
        static_assert(std::is_default_constructible_v<T>, "AtomicQueue::T needs to be default constructible");

        using value_type = T;
        using sequence_t = uint64_t;

        AtomicQueue() = default;

        // disable copy and move
        AtomicQueue(AtomicQueue&&) = delete;

        // try to push a new value into the queue
        template <typename U>
        bool try_push(U&& value) noexcept
        {
            int32_t retry{3};
            auto pos = _head.load(std::memory_order_relaxed);
            for (;;)
            {
                auto& elem = _buffer[idx(pos)];
                auto  currentSequence = turn(pos);
                if (currentSequence == elem.sequence.load(std::memory_order_acquire))
                {
                    if (_head.compare_exchange_strong(pos, pos + 1, std::memory_order::release, std::memory_order::relaxed))
                    {
                        // found the right place
                        // pushing the value into the queue
                        elem.value = std::forward<U>(value);

                        // store the new position as the next sequence
                        elem.sequence.store(currentSequence + 1, std::memory_order_release);

                        // success
                        return true;
                    }
                }
                else
                {
                    const auto prevHead = pos;

                    pos = _head.load(std::memory_order_relaxed);
                    if (pos == prevHead && --retry <= 0)
                    {
                        // queue is probably full, or we simply lost the race...
                        return false;
                    }
                }
            }

            // should be never reached..
            return false;
        }

        // try to pop the next value from the queue
        bool try_pop(value_type& data) noexcept
        {
            int32_t retry{3};
            auto    pos = _tail.load(std::memory_order_relaxed);
            for (;;)
            {
                auto& elem = _buffer[idx(pos)];
                auto  currentSequence = turn(pos);
                if (currentSequence + 1 == elem.sequence.load(std::memory_order_acquire))
                {
                    if (_tail.compare_exchange_strong(pos, pos + 1, std::memory_order::release, std::memory_order::relaxed))
                    {
                        // we got exclusive access to the element
                        data = std::move(elem.value);

                        elem.sequence.store(currentSequence + 2, std::memory_order_release);

                        return true;
                    }
                }
                else
                {
                    auto const prevTail = pos;

                    pos = _tail.load(std::memory_order_relaxed);
                    if (pos == prevTail && --retry <= 0)
                    {
                        // queue is probably empty, or we lost the race...
                        return false;
                    }
                }
            }

            // should be never reached..
            return false;
        }

        [[nodiscard]] bool empty() const noexcept
        { 
            return _head.load(std::memory_order::relaxed) == _tail.load(std::memory_order::relaxed);
        }

        [[nodiscard]] bool full() const noexcept
        {
            return _head.load(std::memory_order::relaxed) - Capacity == _tail.load(std::memory_order::relaxed);
        }

        [[nodiscard]] static constexpr auto capacity() noexcept { return Capacity; }

    private:
        constexpr size_t idx(size_t i) const noexcept { return i & BUFFER_MASK; }
        constexpr size_t turn(size_t i) const noexcept { return (i / Capacity) * 2; }

        struct Element
        {
            std::atomic<sequence_t> sequence{};
            value_type              value{};
        };

        // the buffer mask. Should be for example 0xFFFFF
        static constexpr sequence_t BUFFER_MASK{Capacity - 1};

        // the array of the ringbuffer
        std::array<Element, Capacity> _buffer;

        // the last push position of an element
        alignas(CACHELINE_SIZE) std::atomic<sequence_t> _head{};

        // the following element to pop
        alignas(CACHELINE_SIZE) std::atomic<sequence_t> _tail{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_ATOMIC_QUEUE_HPP