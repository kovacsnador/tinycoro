// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_ATOMIC_QUEUE_HPP
#define TINY_CORO_ATOMIC_QUEUE_HPP

#include <concepts>
#include <array>

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
        using value_type = T;
        using sequence_t = uint64_t;

        AtomicQueue()
        {
            for (sequence_t i = 0; i < _buffer.size(); ++i)
            {
                _buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        // disable copy and move
        AtomicQueue(AtomicQueue&&) = delete;

        // try to push a new value into the queue
        template <typename U>
        bool try_push(U&& value) noexcept
        {
            for (;;)
            {
                // get the current enqueue position
                auto  pos  = _head.load(std::memory_order_relaxed);
                auto& elem = _buffer[pos & BUFFER_MASK];
                auto  seq  = elem.sequence.load(std::memory_order_acquire);

                if (seq == pos)
                {
                    if (_head.compare_exchange_strong(pos, pos + 1, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // found the right place
                        // pushing the value into the queue
                        elem.value = std::forward<U>(value);

                        // store the new position as the next sequence
                        elem.sequence.store(pos + 1, std::memory_order_release);

                        // notify waiter that
                        // we have a new value
                        // in the queue
                        //_head.notify_all();

                        // success
                        return true;
                    }
                }
                else if (pos > seq)
                {
                    // the queue is full
                    // push failed
                    return false;
                }
            }

            // should be never reached..
            return false;
        }

        // try to pop the next value from the queue
        bool try_pop(value_type& data) noexcept
        {
            for (;;)
            {
                // get the current dequeue position
                auto  pos  = _tail.load(std::memory_order_relaxed);
                auto& elem = _buffer[pos & BUFFER_MASK];
                auto  seq  = elem.sequence.load(std::memory_order_acquire);

                if (seq == pos + 1)
                {
                    // we found the right place
                    // try to access the underlying element...
                    if (_tail.compare_exchange_strong(pos, pos + 1, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // we got exclusive access to the element
                        data = std::move(elem.value);

                        elem.sequence.store(pos + SIZE, std::memory_order_release);

                        // notify the waiters
                        // that a value is popped
                        //_tail.notify_all();

                        return true;
                    }
                }
                else if (seq == pos)
                {
                    // the queue is empty
                    // no element can be popped
                    return false;
                }
            }

            // should be never reached..
            return false;
        }

        // Currently not used
        //
        // wait for new element to pop
        // if the queue is empty
        void wait_for_pop() const noexcept
        {
            auto head = _head.load(std::memory_order_relaxed);
            auto tail = _tail.load(std::memory_order_relaxed);

            if (tail == head)
            {
                // if the queue is empty
                // we wait for the next element to pop
                _head.wait(head);
            }
        }

        // Currently not used
        //
        // wait for new element to push
        // in case the queue is full
        void wait_for_push() const noexcept
        {
            auto head = _head.load(std::memory_order_relaxed);
            auto tail = _tail.load(std::memory_order_relaxed);

            if ((head - SIZE) == tail)
            {
                // if the queue is full
                // we wait until someone pops an element
                _tail.wait(tail);
            }
        }

        [[nodiscard]] bool empty() const noexcept { return _head.load(std::memory_order_relaxed) == _tail.load(std::memory_order_relaxed); }

        [[nodiscard]] bool full() const noexcept { return _head.load(std::memory_order_relaxed) - SIZE == _tail.load(std::memory_order_relaxed); }

    private:
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