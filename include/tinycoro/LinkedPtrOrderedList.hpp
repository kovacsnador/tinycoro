// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP
#define TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP

#include <functional>

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
        requires requires (std::remove_pointer_t<NodeT> n) {
            { n.value() < n.value() } -> std::same_as<bool>;
        }
    struct LinkedPtrOrderedList
    {
        using value_type = std::remove_pointer_t<NodeT>;

        // insert a new node
        // and make sure everthing
        // stays in order
        void insert(value_type* node) noexcept
        {
            assert(node);

            _size++;

            value_type** indirect = std::addressof(_root);

            while(*indirect && (*indirect)->value() < node->value())
            {
                // move forward
                indirect = std::addressof((*indirect)->next);
            }

            node->next = std::exchange(*indirect, node);
        }

        // Removes and returns the leading segment (range) of the list
        // whose elements are less than or equal to `valToCompare`.
        [[nodiscard]] value_type* lower_bound(auto valToCompare) noexcept
        {
            auto less_greater = [](auto a, auto b) { return !(a < b); };

            value_type** indirect = std::addressof(_root);
            
            while(*indirect && less_greater(valToCompare, (*indirect)->value()))
            {
                // move forward
                indirect = std::addressof((*indirect)->next);
                _size--;
            }

            if(*indirect != _root)
            {
                return std::exchange(_root, std::exchange(*indirect, nullptr));
            }

            return nullptr;
        }

        bool erase(value_type* elem) noexcept
        {
            assert(elem);

            value_type** indirect = std::addressof(_root);

            while(*indirect)
            {
                if(*indirect == elem)
                {
                    *indirect = elem->next;

                    elem->next = nullptr;
                    --_size;

                    return true;
                }

                indirect = std::addressof((*indirect)->next);
            }

            return false;
        }

        [[nodiscard]] value_type* steal() noexcept
        {
            _size = 0;
            return std::exchange(_root, nullptr);
        }

        [[nodiscard]] auto size() const noexcept { return _size; }

        [[nodiscard]] auto empty() const noexcept { return _size == 0; }

        [[nodiscard]] constexpr value_type* begin() noexcept { return _root; }

    private:
        value_type* _root{};
        size_t      _size{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP