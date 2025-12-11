// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_DISPATCHER_HPP
#define TINY_CORO_DISPATCHER_HPP

#include <atomic>
#include <concepts>

#include "Common.hpp"

namespace tinycoro { namespace detail {

    namespace local {
        template <template <typename> class AtomT, typename T>
            requires std::unsigned_integral<T>
        void Notify(AtomT<T>& atom) noexcept
        {
            atom.fetch_add(1, std::memory_order::release);
            atom.notify_all();
        }
    } // namespace local

    template <typename QueueT>
    struct Dispatcher
    {
        using value_type = typename QueueT::value_type;

        explicit Dispatcher(QueueT& queue)
        : _queue{queue}
        {
        }

        [[nodiscard]] auto push_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _pushEvent.load(order); }
        [[nodiscard]] auto pop_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _popEvent.load(order); }

        void wait_for_push(auto prevState) const noexcept
        {
            if (full())
                _popEvent.wait(prevState);
        }

        void wait_for_pop(auto prevState) const noexcept
        {
            if (empty())
                _pushEvent.wait(prevState);
        }

        void notify_push_waiters() noexcept { local::Notify(_pushEvent); }

        void notify_pop_waiters() noexcept { local::Notify(_popEvent); }

        template <typename T>
        [[nodiscard]] auto try_push(T&& elem) noexcept
        {
            if (_queue.try_push(std::forward<T>(elem)))
            {
                // push was ok
                local::Notify(_pushEvent);
                return true;
            }

            // every queue is full
            return false;
        }

        [[nodiscard]] bool try_pop(auto& elem) noexcept
        {
            if (_queue.try_pop(elem))
            {
                // pop was ok
                local::Notify(_popEvent);
                return true;
            }

            // every queue is empty
            return false;
        }

        [[nodiscard]] auto full() const noexcept { return _queue.full(); }
        [[nodiscard]] auto empty() const noexcept { return _queue.empty(); }

    private:
        // uint32_t for safe overflow
        std::atomic<uint32_t> _pushEvent;
        std::atomic<uint32_t> _popEvent;

        QueueT& _queue;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_DISPATCHER_HPP