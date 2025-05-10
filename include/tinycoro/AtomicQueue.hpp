#ifndef TINY_CORO_ATOMIC_QUEUE_HPP
#define TINY_CORO_ATOMIC_QUEUE_HPP

#include <concepts>

#include "CachelineAlign.hpp"
#include "Common.hpp"

namespace tinycoro { namespace detail {

    // This is a multi-producer multi-consumer
    // lockfree queue.
    // Inside uses a ringbuffer therefor SIZE need to be a power of 2.
    template <typename T, uint64_t SIZE>
        requires detail::IsPowerOf2<SIZE>::value
    class AtomicQueue
    {
    public:
        using value_type = T;
        using sequence_t = uint64_t;

        AtomicQueue()
        : _buffer{std::make_unique<Element[]>(SIZE)}
        {
            for (uint64_t i = 0; i < SIZE; ++i)
            {
                auto elem = _buffer.get() + i;
                elem->_sequence.store(i, std::memory_order_relaxed);
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
                auto pos  = _head.load(std::memory_order_relaxed);
                auto elem = _buffer.get() + (pos & BUFFER_MASK);
                auto seq  = elem->_sequence.load(std::memory_order_acquire);

                if (seq == pos)
                {
                    if (_head.compare_exchange_strong(pos, pos + 1, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // found the right place
                        // pushing the value into the queue
                        elem->_value = std::forward<U>(value);

                        // store the new position as the next sequence
                        elem->_sequence.store(pos + 1, std::memory_order_release);

                        // notify waiter that
                        // we have a new value
                        // in the queue
                        _head.notify_all();

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
                auto pos  = _tail.load(std::memory_order_relaxed);
                auto elem = _buffer.get() + (pos & BUFFER_MASK);
                auto seq  = elem->_sequence.load(std::memory_order_acquire);

                if (seq == pos + 1)
                {
                    // we found the right place
                    // try to access the underlying element...
                    if (_tail.compare_exchange_strong(pos, pos + 1, std::memory_order_release, std::memory_order_relaxed))
                    {
                        // we got exclusive access to the element
                        data = std::move(elem->_value);

                        elem->_sequence.store(pos + SIZE, std::memory_order_release);

                        // notify the waiters
                        // that a value is popped
                        _tail.notify_all();

                        return true;
                    }
                }
                else if (seq < pos + 1)
                {
                    // the queue is empty
                    // no element can be popped
                    return false;
                }
            }

            // should be never reached..
            return false;
        }

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
            std::atomic<sequence_t> _sequence{};
            value_type              _value{};
        };

        // the buffer mask. Should be for example 0xFFFFF
        static constexpr sequence_t BUFFER_MASK{SIZE - 1};

        // the pointer of the ringbuffer
        std::unique_ptr<Element[]> _buffer;

        // the last push position of an element
        alignas(CACHELINE_SIZE) std::atomic<sequence_t> _head{};

        // the following element to pop
        alignas(CACHELINE_SIZE) std::atomic<sequence_t> _tail{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_ATOMIC_QUEUE_HPP