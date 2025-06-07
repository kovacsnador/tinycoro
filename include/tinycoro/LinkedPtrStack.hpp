// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINEKD_PTR_STACK_HPP
#define TINY_CORO_LINEKD_PTR_STACK_HPP

#include "Common.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
    struct LinkedPtrStack
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push(value_type* newNode) noexcept
        {
            assert(newNode);

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(newNode && newNode->owner == nullptr);
            newNode->owner = this;
#endif
            ++_size;

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if (_top)
                    _top->prev = newNode;

                newNode->prev = nullptr;
            }

            if(_top == nullptr)
                _end = newNode;

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

#ifdef TINYCORO_DIAGNOSTICS
                elem->owner = nullptr;
#endif

                if(_top == nullptr)
                    _end = nullptr;
            }

            return elem;
        }

        [[nodiscard]] auto size() const noexcept { return _size; }

        [[nodiscard]] constexpr value_type* top() noexcept { return _top; }
        [[nodiscard]] constexpr value_type* last() noexcept { return _end; }

        [[nodiscard]] constexpr bool empty() const noexcept { return !_top; }

        [[nodiscard]] value_type* steal() noexcept
        {
#ifdef TINYCORO_DIAGNOSTICS
            ClearOwners(_top, this);
#endif
            _size = 0;
            _end = nullptr;
            return std::exchange(_top, nullptr);
        }

        bool erase(value_type* elem) noexcept
        {
            assert(elem);

            if(elem->next == nullptr && elem != _end)
                return false;

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if(_top == nullptr)
                    return false;

#ifdef TINYCORO_DIAGNOSTICS
                TINYCORO_ASSERT(elem != nullptr && elem->owner == this);

                elem->owner = nullptr;
#endif

                if (elem->next)
                    elem->next->prev = elem->prev;
                else
                    _end = elem->prev;

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
                value_type* prev{nullptr};
                while (*indirect)
                {
                    if (*indirect == elem)
                    {
                        // found our element
                        //
                        // take out from the list
                        *indirect  = elem->next;
                        elem->next = nullptr;
                        
                        if(elem == _end)
                            _end = prev;

                        --_size;

#ifdef TINYCORO_DIAGNOSTICS
                        elem->owner = nullptr;
#endif
                        return true;
                    }

                    prev = *indirect;

                    indirect = std::addressof((*indirect)->next);
                }
            }

            return false;
        }

    private:
        value_type* _top{nullptr};
        value_type* _end{nullptr};

        size_t      _size{};
    };
}} // namespace tinycoro::detail

#endif // TINY_CORO_LINEKD_PTR_STACK_HPP