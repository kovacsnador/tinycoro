// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP
#define TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP

#include "Common.hpp"
#include "LinkedUtils.hpp"
#include "Diagnostics.hpp"


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

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(node && node->owner == nullptr);
            node->owner = this;
#endif
            _size++;

            value_type** indirect = std::addressof(_root);

            while(*indirect && (*indirect)->value() < node->value())
            {
                // move forward
                indirect = std::addressof((*indirect)->next);
            }

            node->next = std::exchange(*indirect, node);

            if(node->next == nullptr)
                _last = node;
        }

        // Removes and returns the leading segment (range) of the list
        // whose elements are less than or equal to `valToCompare`.
        [[nodiscard]] value_type* lower_bound(auto valToCompare) noexcept
        {
            auto less_greater = [](auto a, auto b) { return !(a < b); };

            value_type** indirect = std::addressof(_root);
            
            while(*indirect && less_greater(valToCompare, (*indirect)->value()))
            {
#ifdef TINYCORO_DIAGNOSTICS
                // detach from owner list
                (*indirect)->owner = nullptr;
#endif
                // move forward
                indirect = std::addressof((*indirect)->next);
                _size--;
            }

            if(_size == 0)
                _last = nullptr;

            if(*indirect != _root)
            {
                return std::exchange(_root, std::exchange(*indirect, nullptr));
            }

            return nullptr;
        }

        bool erase(value_type* elem) noexcept
        {
            assert(elem);

            if(elem->next == nullptr && elem != _last)
                return false;

            value_type** indirect = std::addressof(_root);
            value_type* prev{nullptr};

            while(*indirect)
            {
                if(*indirect == elem)
                {
                    *indirect = elem->next;

                    elem->next = nullptr;
#ifdef TINYCORO_DIAGNOSTICS
                    elem->owner = nullptr;
#endif
                    if(elem == _last)
                        _last = prev;

                    --_size;

                    return true;
                }

                prev = *indirect;
                indirect = std::addressof((*indirect)->next);
            }

            return false;
        }

        [[nodiscard]] value_type* steal() noexcept
        {
#ifdef TINYCORO_DIAGNOSTICS
            ClearOwners(_root, this);
#endif
            _size = 0;
            _last = nullptr;
            return std::exchange(_root, nullptr);
        }

        [[nodiscard]] auto size() const noexcept { return _size; }

        [[nodiscard]] auto empty() const noexcept { return _size == 0; }

        [[nodiscard]] constexpr value_type* begin() noexcept { return _root; }
        [[nodiscard]] constexpr value_type* last() noexcept { return _last; }

    private:
        value_type* _root{};
        value_type* _last{};
        
        size_t      _size{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP