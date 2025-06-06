// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINKED_PTR_LIST_HPP
#define TINY_CORO_LINKED_PTR_LIST_HPP

#include "Common.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro { namespace detail {

    template <concepts::DoubleLinkable NodeT>
    struct LinkedPtrList
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push_front(value_type* newNode) noexcept
        {
            assert(newNode);

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(newNode && newNode->owner == nullptr);
            newNode->owner = this;
#endif

            if(_first)
                _first->prev = newNode;
            else
                _last = newNode;

            newNode->next = std::exchange(_first, newNode);
            _first->prev = nullptr;

            ++_size;
        }

        bool erase(value_type* node) noexcept
        {
            assert(node);
            assert(_size);

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(node && node->owner == this);
#endif
            if(node->next == nullptr && node != _last)
                return false;   // not in the list

            if(node->next)
                node->next->prev = node->prev;
            else
                _last = node->prev;

            if(node->prev)
                node->prev->next = node->next;
            else
                _first = node->next;

            node->prev = nullptr;
            node->next = nullptr;

#ifdef TINYCORO_DIAGNOSTICS
            node->owner = nullptr;
#endif
            --_size;

            return true;
        }

        [[nodiscard]] value_type* begin() const noexcept { return _first; }
        [[nodiscard]] value_type* last() const noexcept { return _last; }
        [[nodiscard]] bool        empty() const noexcept { return !_first; }
        [[nodiscard]] auto        size() const noexcept { return _size; }

    private:
        value_type* _first{nullptr};
        value_type* _last{nullptr};

        size_t      _size{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_LINKED_PTR_LIST_HPP