#ifndef __TINY_CORO_LINKED_PTR_QUEUE_HPP__
#define __TINY_CORO_LINKED_PTR_QUEUE_HPP__

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template<concepts::Linkable NodeT>
    struct LinkedPtrQueue
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push(value_type* newNode)
        {
            assert(newNode);
            assert(newNode->next == nullptr);

            ++_size;

            if(_last)
            {
                _last->next = newNode;
            }
            else
            {
                _first = newNode;
            }

            _last = newNode;
        }

        // Pops and return the poped node.
        [[nodiscard]] value_type* pop()
        {
            auto top = _first;
            if (top)
            {
                --_size;
                _first = _first->next;

                if(_first == nullptr)
                {
                    _last = nullptr;
                }

            }
            return top;
        }

        [[nodiscard]] auto size() const noexcept { return _size; }

        [[nodiscard]] constexpr value_type* begin() noexcept { return _first; }

        [[nodiscard]] constexpr bool empty() const noexcept { return !_first; }

        [[nodiscard]] value_type* steal() noexcept
        {
            _size = 0;
            _last = nullptr;
            return std::exchange(_first, nullptr);
        }

    private:
        value_type* _first{nullptr};
        value_type* _last{nullptr};
        size_t      _size{};
    };

}} // namespace tinycoro::detail

#endif // !__TINY_CORO_LINKED_PTR_QUEUE_HPP__