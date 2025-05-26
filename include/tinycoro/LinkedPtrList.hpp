#ifndef TINY_CORO_LINKED_PTR_LIST_HPP
#define TINY_CORO_LINKED_PTR_LIST_HPP

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template <concepts::DoubleLinkable NodeT>
    struct LinkedPtrList
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push_front(value_type* newNode) noexcept
        {
            assert(newNode);

            if(_first)
                _first->prev = newNode;

            newNode->next = std::exchange(_first, newNode);
            _first->prev = nullptr;

            ++_size;
        }

        void erase(value_type* node) noexcept
        {
            assert(node);
            assert(_size);

            if(node->next)
                node->next->prev = node->prev;

            if(node->prev)
                node->prev->next = node->next;
            else
                _first = node->next;

            node->prev = nullptr;
            node->next = nullptr;

            --_size;
        }

        [[nodiscard]] value_type* begin() const noexcept { return _first; }
        [[nodiscard]] bool        empty() const noexcept { return !_first; }
        [[nodiscard]] auto        size() const noexcept { return _size; }

    private:
        value_type* _first{nullptr};
        size_t      _size{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_LINKED_PTR_LIST_HPP