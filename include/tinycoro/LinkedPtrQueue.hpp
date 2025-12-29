// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINKED_PTR_QUEUE_HPP
#define TINY_CORO_LINKED_PTR_QUEUE_HPP

#include "Common.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
    struct LinkedPtrQueue
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push(value_type* newNode) noexcept
        {
            assert(newNode);
            assert(newNode->next == nullptr);

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(newNode && newNode->owner == nullptr);
            newNode->owner = this;
#endif
            
            ++_size;

            if (_last)
            {
                _last->next = newNode;

                if constexpr (concepts::DoubleLinkable<NodeT>)
                {
                    newNode->prev = _last;
                }
            }
            else
            {
                _first = newNode;
            }

            _last = newNode;
        }

        void push_front(value_type* newNode) noexcept
        {
            assert(newNode);
            assert(newNode->next == nullptr);

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(newNode && newNode->owner == nullptr);
            newNode->owner = this;
#endif
            ++_size;

            newNode->next = std::exchange(_first, newNode);

            if (_last == nullptr)
                _last = _first;

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                _first->prev = nullptr;

                if (_first->next)
                    _first->next->prev = _first;
            }
        }

        // Pops and return the popped node.
        [[nodiscard]] value_type* pop() noexcept
        {
            if (_first)
            {
                --_size;

                auto top = std::exchange(_first, _first->next);

                top->next = nullptr;
#ifdef TINYCORO_DIAGNOSTICS
                top->owner = nullptr;
#endif
                if (_first == nullptr)
                {
                    _last = nullptr;
                }
                else
                {
                    if constexpr (concepts::DoubleLinkable<NodeT>)
                    {
                        _first->prev = nullptr;
                    }
                }

                return top;
            }

            return nullptr;
        }

        [[nodiscard]] auto size() const noexcept { return _size; }

        [[nodiscard]] constexpr value_type* begin() noexcept { return _first; }
        [[nodiscard]] constexpr value_type* last() noexcept { return _last; }

        [[nodiscard]] constexpr bool empty() const noexcept { return !_first; }

        [[nodiscard]] value_type* steal() noexcept
        {
#ifdef TINYCORO_DIAGNOSTICS
            ClearOwners(_first, this);
#endif
            _size = 0;
            _last = nullptr;
            return std::exchange(_first, nullptr);
        }

        [[nodiscard]] bool erase(value_type* elem) noexcept
        {
            assert(elem);

            if(elem->next == nullptr && elem != _last)
                return false;

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if(_first == nullptr)
                    return false;

#ifdef TINYCORO_DIAGNOSTICS
                TINYCORO_ASSERT(elem && elem->owner == this);
#endif      
                // debug check if the elem is in list
                assert(detail::helper::Contains(_first, elem));

                if (elem->next)
                    elem->next->prev = elem->prev;
                else
                    _last = elem->prev;

                if (elem->prev)
                    elem->prev->next = elem->next;
                else
                    _first = elem->next;

                elem->prev = nullptr;
                elem->next = nullptr;
#ifdef TINYCORO_DIAGNOSTICS
                elem->owner = nullptr;
#endif
                --_size;

                return true;
            }
            else
            {
                value_type** indirect = std::addressof(_first);
                value_type*  prev{nullptr};

                while (*indirect)
                {
                    if (*indirect == elem)
                    {
                        // take out from the list
                        *indirect = elem->next;

                        elem->next = nullptr;

                        if (elem == _last)
                            _last = prev;

                        --_size;

#ifdef TINYCORO_DIAGNOSTICS
                        elem->owner = nullptr;
#endif 
                        return true;
                    }

                    prev     = *indirect;
                    indirect = std::addressof((*indirect)->next);
                }
            }

            return false;
        }

    private:
        value_type* _first{nullptr};
        value_type* _last{nullptr};

        size_t      _size{};
    };

}} // namespace tinycoro::detail

#endif // !TINY_CORO_LINKED_PTR_QUEUE_HPP