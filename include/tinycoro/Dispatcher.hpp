// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_DISPATCHER_HPP
#define TINY_CORO_DISPATCHER_HPP

#include <cassert>

namespace tinycoro { namespace detail {
    template <typename WorkerT>
    struct Dispatcher
    {
        Dispatcher(std::span<WorkerT> workers)
        : _workers{workers}
        {
            assert(_workers.size());
        }

        template <typename T>
        void Push(T&& task) noexcept
        {
            // forward it to the next worker
            _workers[_current++]->Push(std::forward<T>(task));

            if (_current >= _workers.size())
                _current = 0;
        }

        bool Redistribute(auto worker) noexcept
        {
            for (auto& it : _workers)
            {
                auto [list1, list2] = it->Pull();

                if (list1 || list2)
                {
                    _Distribute(list1, worker, it);
                    _Distribute(list2, worker, it);

                    return true;
                }
            }

            return false;
        }

        void NotifyAll() noexcept
        {
            std::ranges::for_each(_workers, [](auto& it) { it->Notify(); });
        }

    private:
        void _Distribute(auto head, auto& w1, auto& w2)
        {
            while (head)
            {
                auto next = std::exchange(head->next, nullptr);
                w1->Push(head);
                head = next;

                if (head)
                {
                    auto next = std::exchange(head->next, nullptr);
                    w2->Push(head);
                    head = next;
                }
            }
        }

        std::span<WorkerT> _workers;
        size_t             _current{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_DISPATCHER_HPP