// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_DISPATCHER_HPP
#define TINY_CORO_DISPATCHER_HPP

#include <atomic>
#include <concepts>

#include "Common.hpp"
#include "CachelineAlign.hpp"

namespace tinycoro { namespace detail {

    namespace local {

        template<template <typename> class AtomT, typename T>
        using notifyFunc_t = void(*)(AtomT<T>*);

        template <template <typename> class AtomT, typename T>
            requires std::unsigned_integral<T>
        void Notify(AtomT<T>& atom, notifyFunc_t<AtomT, T> notifyFunc, std::memory_order order = std::memory_order::release) noexcept
        {
            atom.fetch_add(1, order);
            notifyFunc(std::addressof(atom));
        }
    } // namespace local

    template <typename QueueT>
    struct Dispatcher
    {
        using value_type = typename QueueT::value_type;
        using state_type = size_t;

        explicit Dispatcher(QueueT& queue)
        : _queue{queue}
        {
        }

        [[nodiscard]] auto push_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _pushEvent.load(order); }
        [[nodiscard]] auto pop_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _popEvent.load(order); }

        void wait_for_push(state_type prevState) const noexcept
        {
            _popEvent.wait(prevState);
        }

        void wait_for_pop(state_type prevState) const noexcept
        {
            _pushEvent.wait(prevState);
        }

        void notify_push_waiters() noexcept { local::Notify(_pushEvent, std::atomic_notify_all); }

        void notify_pop_waiters() noexcept { local::Notify(_popEvent, std::atomic_notify_all); }

        template <typename T>
        [[nodiscard]] auto try_push(T&& elem) noexcept
        {
            if (_queue.try_push(std::forward<T>(elem)))
            {
                // push was ok
                local::Notify(_pushEvent, std::atomic_notify_all);
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
                local::Notify(_popEvent, std::atomic_notify_all);
                return true;
            }

            // every queue is empty
            return false;
        }

        [[nodiscard]] auto full() const noexcept { return _queue.full(); }
        [[nodiscard]] auto empty() const noexcept { return _queue.empty(); }

    private:
        // uint32_t for safe overflow
        alignas(CACHELINE_SIZE) std::atomic<state_type> _pushEvent;
        alignas(CACHELINE_SIZE) std::atomic<state_type> _popEvent;

        QueueT& _queue;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_DISPATCHER_HPP