#ifndef __TINY_CORO_LINKED_PTR_LIST_HPP__
#define __TINY_CORO_LINKED_PTR_LIST_HPP__

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template <concepts::DoubleLinkable NodeT>
    struct LinkedPtrList
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push_front(value_type* newNode) noexcept
        {
            assert(newNode);

            if (_first)
            {
                newNode->next = _first;
                _first->prev  = newNode;
                _first        = newNode;
            }
            else
            {
                _first = newNode;
            }

            ++_size;
        }

        void erase(value_type* node) noexcept
        {
            assert(node);

            if (node == _first)
            {
                _first = node->next;

                if (_first)
                {
                    _first->prev = nullptr;
                }
            }
            else
            {
                if (node->next)
                {
                    node->next->prev = node->prev;
                }

                if (node->prev)
                {
                    node->prev->next = node->next;
                }
            }

            node->prev = nullptr;
            node->next = nullptr;

            --_size;
        }

        [[nodiscard]] value_type* begin() const noexcept { return _first; }
        [[nodiscard]] bool empty() const noexcept { return !_first; }
        [[nodiscard]] auto size() const noexcept { return _size; }

    private:
        value_type* _first{nullptr};
        size_t _size{};
    };

}} // namespace tinycoro::detail

#endif //!__TINY_CORO_LINKED_PTR_LIST_HPP__