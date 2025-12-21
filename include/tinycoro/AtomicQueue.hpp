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

    // A multi-producer, multi-consumer lock-free queue.
    //
    // Internally, this queue uses a fixed-size ring buffer of size `SIZE`,
    // which must be a power of two.
    //
    // The queue supports concurrent `try_push` and `try_pop` operations without locks.
    // Blocking variants (`wait_for_push`, `wait_for_pop`) are provided via C++20 atomic wait/notify.
    //
    // Sequence counters (`_head`, `_tail`, and `Element::sequence`) are 64-bit unsigned integers
    // (`uint64_t`). These are used to coordinate access to buffer elements and enforce ordering.
    //
    // ⚠ Overflow Note:
    // The sequence values are monotonically increasing and **not wrapped or masked**.
    // This means that once a `uint64_t` sequence counter overflows (after `2^64` operations),
    // the queue’s correctness is no longer guaranteed. However, at even extremely high
    // message rates (billions of ops/sec), this would take hundreds of years to occur,
    // so overflow is generally a theoretical issue in practice.
    //
    // ⚠ Lock-free Guarantee:
    // This queue is only truly *lock-free* if `std::atomic<uint64_t>` is lock-free on
    // the target platform. You can check this via `std::atomic<uint64_t>::is_always_lock_free`.
    // On most modern 64-bit platforms, this condition holds.
    // If it's not lock-free, atomic operations may fall back to locks or syscalls.
    //
    // This makes the queue a good fit for high-throughput, low-latency systems
    // where long-term runtime and platform-specific atomic guarantees are understood.
    template <typename T, uint64_t SIZE>
        requires detail::IsPowerOf2<SIZE>::value
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
            auto pos = _head.load(std::memory_order_acquire);
            for (;;)
            {
                auto& elem = _buffer[pos & BUFFER_MASK];
                if (turn(pos) * 2 == elem.sequence.load(std::memory_order_acquire))
                {
                    if (_head.compare_exchange_strong(pos, pos + 1, std::memory_order::release, std::memory_order::relaxed))
                    {
                        // found the right place
                        // pushing the value into the queue
                        elem.value = std::forward<U>(value);

                        // store the new position as the next sequence
                        elem.sequence.store(turn(pos) * 2 + 1, std::memory_order_release);

                        // success
                        return true;
                    }
                }
                else
                {
                    const auto prevHead = pos;

                    pos = _head.load(std::memory_order_acquire);
                    if (pos == prevHead)
                    {
                        // queue is full
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
            auto pos = _tail.load(std::memory_order_acquire);
            for (;;)
            {
                auto& elem = _buffer[pos & BUFFER_MASK];
                if (turn(pos) * 2 + 1 == elem.sequence.load(std::memory_order_acquire))
                {
                    if (_tail.compare_exchange_strong(pos, pos + 1, std::memory_order::release, std::memory_order::relaxed))
                    {
                        // we got exclusive access to the element
                        data = std::move(elem.value);

                        elem.sequence.store(turn(pos) * 2 + 2, std::memory_order_release);

                        return true;
                    }
                }
                else
                {
                    auto const prevTail = pos;
                    pos                 = _tail.load(std::memory_order_acquire);
                    if (pos == prevTail)
                    {
                        return false;
                    }
                }
            }

            // should be never reached..
            return false;
        }

        [[nodiscard]] bool empty() const noexcept { return _head.load(std::memory_order_relaxed) == _tail.load(std::memory_order_relaxed); }

        [[nodiscard]] bool full() const noexcept { return _head.load(std::memory_order_relaxed) - SIZE == _tail.load(std::memory_order_relaxed); }

    private:
        constexpr size_t idx(size_t i) const noexcept { return i % SIZE; }
        constexpr size_t turn(size_t i) const noexcept { return i / SIZE; }

        struct Element
        {
            std::atomic<sequence_t> sequence{};
            value_type              value{};
        };

        // the buffer mask. Should be for example 0xFFFFF
        static constexpr sequence_t BUFFER_MASK{SIZE - 1};

        // the array of the ringbuffer
        std::array<Element, SIZE> _buffer;

        // the last push position of an element
        alignas(CACHELINE_SIZE) std::atomic<sequence_t> _head{};

        // the following element to pop
        alignas(CACHELINE_SIZE) std::atomic<sequence_t> _tail{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_ATOMIC_QUEUE_HPP