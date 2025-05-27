// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINKED_PTR_QUEUE_HPP
#define TINY_CORO_LINKED_PTR_QUEUE_HPP

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
    struct LinkedPtrQueue
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push(value_type* newNode) noexcept
        {
            assert(newNode);
            assert(newNode->next == nullptr);

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

        void concat(LinkedPtrQueue& other) noexcept
        {
            if (_first == nullptr)
            {
                _first = other._first;
                _last  = other._last;
            }
            else
            {
                if constexpr (concepts::DoubleLinkable<NodeT>)
                {
                    if (other._first)
                        other._first->prev = _last;
                }

                if (_last)
                    _last->next = other._first;

                if (other._last)
                    _last = other._last;
            }

            _size += other._size;

            // reset the other queue
            std::ignore = other.steal();
        }

        // Pops and return the popped node.
        [[nodiscard]] value_type* pop() noexcept
        {
            if (_first)
            {
                --_size;

                auto top = std::exchange(_first, _first->next);

                top->next = nullptr;

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

        [[nodiscard]] constexpr bool empty() const noexcept { return !_first; }

        [[nodiscard]] value_type* steal() noexcept
        {
            _size = 0;
            _last = nullptr;
            return std::exchange(_first, nullptr);
        }

        bool erase(value_type* elem)
        {
            assert(elem);

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if(_first == nullptr)
                    return false;

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