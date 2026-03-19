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

        template <template <typename> class AtomT, typename T>
        using notifyFunc_t = void (*)(AtomT<T>*);

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
        [[nodiscard]] state_type push_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _popEvent.load(order); }

        void wait_for_push(state_type state) const noexcept
        {
            if (state == _pushEvent.load(std::memory_order::relaxed) /*isFull*/ && _stopToken.stop_requested() == false)
                _popEvent.wait(state);
        }

        void wait_for_pop(state_type state) const noexcept
        {
            if (state + QueueT::capacity() == _popEvent.load(std::memory_order::relaxed) /* isEmpty */ && _stopToken.stop_requested() == false)
                _pushEvent.wait(state);
        }

        void notify_all() noexcept
        {
            local::Notify(_pushEvent, std::atomic_notify_all);
            local::Notify(_popEvent, std::atomic_notify_all);
        }

        void notify_push() noexcept
        {
            _pushEvent.notify_all();
        }

        auto increase_task_counter(size_t n, std::memory_order order = std::memory_order::relaxed) noexcept
        {
            return _taskCounter.fetch_add(n, order);
        }

        auto decrease_task_counter(size_t n, std::memory_order order = std::memory_order::relaxed) noexcept
        {
            auto before = _taskCounter.fetch_sub(n, order);
            
            assert(n <= before);

            if(before == 1)
            {
                // in case this was the last task
                // we notify everybody, that they not stay
                // in a waiting state.
                notify_all();
            }

            return before;
        }

        auto task_counter(std::memory_order order = std::memory_order::relaxed) const noexcept
        {
            return _taskCounter.load(order);
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
        std::atomic<size_t> _taskCounter{0};
    };

    template <typename QueueT, typename TaskT>
    struct ConcurrentDispatcher
    {
        using value_type   = TaskT;
        using dispatcher_t = detail::Dispatcher<QueueT>;

        static_assert(std::same_as<decltype(QueueT::capacity()), typename dispatcher_t::state_type>,
                      "QueueT::capacity() need to match with dispatcher_t::state_type");

        ConcurrentDispatcher(QueueT& queue, std::stop_token stopToken)
        : _dispatcher{queue, std::move(stopToken)}
        {
        }

        ~ConcurrentDispatcher()
        {
            // destroy remaining tasks in the queue.
            typename TaskT::element_type* ptr{};
            while (_dispatcher.try_pop(ptr))
            {
                TaskT destroyer{ptr};
            }
        }

        [[nodiscard]] auto pop_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _dispatcher.pop_state(order); }
        [[nodiscard]] auto push_state(std::memory_order order = std::memory_order::relaxed) const noexcept { return _dispatcher.push_state(order); }

        void wait_for_push(dispatcher_t::state_type state) const noexcept { _dispatcher.wait_for_push(state); }
        void wait_for_pop(dispatcher_t::state_type state) const noexcept { _dispatcher.wait_for_pop(state); }

        void notify_all() noexcept { _dispatcher.notify_all(); }

        [[nodiscard]] auto try_push(TaskT&& elem) noexcept
        {
            auto ptr = elem.release();

            if (_dispatcher.try_push(ptr))
                return true;

            elem.reset(ptr);
            return false;
        }

        [[nodiscard]] bool try_pop(TaskT& elem) noexcept
        {
            typename TaskT::element_type* ptr{};

            auto succeed = _dispatcher.try_pop(ptr);
            if (succeed)
                elem.reset(ptr);

            return succeed;
        }

        auto increase_task_counter(size_t n, std::memory_order order = std::memory_order::relaxed) noexcept
        {
            return _dispatcher.increase_task_counter(n, order);
        }

        auto decrease_task_counter(size_t n, std::memory_order order = std::memory_order::relaxed) noexcept
        {
            assert(n <= task_counter());
            return _dispatcher.decrease_task_counter(n, order);
        }

        auto task_counter(std::memory_order order = std::memory_order::relaxed) const noexcept
        {
            return _dispatcher.task_counter(order);
        }

    private:
        detail::Dispatcher<QueueT> _dispatcher;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_DISPATCHER_HPP