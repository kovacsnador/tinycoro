// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINEKD_PTR_STACK_HPP
#define TINY_CORO_LINEKD_PTR_STACK_HPP

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
    struct LinkedPtrStack
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push(value_type* newNode) noexcept
        {
            assert(newNode);

            ++_size;

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if (_top)
                    _top->prev = newNode;

                newNode->prev = nullptr;
            }

            newNode->next = std::exchange(_top, newNode);
        }

        // Pops and return the poped node.
        [[nodiscard]] value_type* pop() noexcept
        {
            value_type* elem{nullptr};

            if (_top)
            {
                elem = std::exchange(_top, _top->next);
                elem->next = nullptr;

                if constexpr (concepts::DoubleLinkable<NodeT>)
                {
                    if (_top)
                        _top->prev = nullptr;

                    // clean up the element
                    elem->prev = nullptr;
                }

                --_size;
            }

            return elem;
        }

        [[nodiscard]] auto size() const noexcept { return _size; }

        [[nodiscard]] constexpr value_type* top() noexcept { return _top; }

        [[nodiscard]] constexpr bool empty() const noexcept { return !_top; }

        [[nodiscard]] value_type* steal() noexcept
        {
            _size = 0;
            return std::exchange(_top, nullptr);
        }

        bool erase(value_type* elem) noexcept
        {
            assert(elem);

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if(_top == nullptr)
                    return false;

                if (elem->next)
                    elem->next->prev = elem->prev;

                if (elem->prev)
                    elem->prev->next = elem->next;
                else
                    _top = elem->next;

                elem->next = nullptr;
                elem->prev = nullptr;

                --_size;
                return true;
            }
            else
            {
                value_type** indirect = std::addressof(_top);
                while (*indirect)
                {
                    if (*indirect == elem)
                    {
                        // found our element
                        //
                        // take out from the list
                        *indirect  = elem->next;
                        elem->next = nullptr;

                        --_size;
                        return true;
                    }
                    indirect = std::addressof((*indirect)->next);
                }
            }

            return false;
        }

    private:
        value_type* _top{nullptr};
        size_t      _size{};
    };
}} // namespace tinycoro::detail

#endif // TINY_CORO_LINEKD_PTR_STACK_HPP