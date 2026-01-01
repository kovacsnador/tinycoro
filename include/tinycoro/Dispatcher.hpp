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

        // size_t for safe overflow
        using state_type = size_t;

        static_assert(std::unsigned_integral<state_type>, "state_type needs to be unsigned for safe overflow");

        explicit Dispatcher(QueueT& queue, std::stop_token stopToken)
        : _queue{queue}
        , _stopToken{std::move(stopToken)}
        {
        }

        [[nodiscard]] state_type pop_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _pushEvent.load(order); }

        void wait_for_push() const noexcept
        {  
            auto state = _popEvent.load(std::memory_order::acquire);
            if (state == _pushEvent.load(std::memory_order::relaxed) /*isFull*/ && _stopToken.stop_requested() == false)
                _popEvent.wait(state);
        }

        void wait_for_pop(state_type state) const noexcept
        {
            if (state + _queue.capacity() == _popEvent.load(std::memory_order::relaxed) /* isEmpty */ && _stopToken.stop_requested() == false)
                _pushEvent.wait(state);
        }

        void wait_for_pop() const noexcept
        {
            auto state = _pushEvent.load(std::memory_order::acquire);
            wait_for_pop(state);
        }

        // TODO: remove this if it's working.
        //void notify_push_waiters() noexcept { local::Notify(_popEvent, std::atomic_notify_all); }
        //void notify_pop_waiters() noexcept { local::Notify(_pushEvent, std::atomic_notify_all); }

        void notify_all() noexcept
        {
            local::Notify(_pushEvent, std::atomic_notify_all);
            local::Notify(_popEvent, std::atomic_notify_all);

            //notify_pop_waiters();
            //notify_push_waiters();
        }

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

        [[nodiscard]] auto full() const noexcept { return _pushEvent.load(std::memory_order::relaxed) == _popEvent.load(std::memory_order::relaxed); }
        
        [[nodiscard]] auto empty() const noexcept
        {
            auto push = _pushEvent.load(std::memory_order::relaxed) + QueueT::capacity();
            auto pop  = _popEvent.load(std::memory_order::relaxed);

            return push == pop;
        }

    private:
        alignas(CACHELINE_SIZE) std::atomic<state_type> _pushEvent{};
        alignas(CACHELINE_SIZE) std::atomic<state_type> _popEvent{QueueT::capacity()};

        QueueT& _queue;

        std::stop_token _stopToken;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_DISPATCHER_HPP