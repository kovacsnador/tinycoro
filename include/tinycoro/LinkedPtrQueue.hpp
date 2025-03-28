#ifndef __TINY_CORO_LINKED_PTR_QUEUE_HPP__
#define __TINY_CORO_LINKED_PTR_QUEUE_HPP__

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

            if (_first)
            {
                newNode->next = _first;

                if constexpr (concepts::DoubleLinkable<NodeT>)
                {
                    _first->prev = newNode;
                }

                _first = newNode;
            }
            else
            {
                _first = newNode;
                _last = newNode;
            }

            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                newNode->prev = nullptr;
            }
        }


        void concat(LinkedPtrQueue& other)
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

        // Pops and return the poped node.
        [[nodiscard]] value_type* pop() noexcept
        {
            auto top = _first;
            if (top)
            {
                --_size;
                _first = _first->next;

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

                top->next = nullptr;
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

        bool erase(value_type* elem)
        {
            if constexpr (concepts::DoubleLinkable<NodeT>)
            {
                if (elem)
                {
                    // debug check if the elem is in list
                    assert(detail::helper::Contains(_first, elem));

                    if (elem == _first)
                    {
                        _first = _first->next;

                        if (_first)
                        {
                            _first->prev = nullptr;
                        }
                        else
                        {
                            _last = nullptr;
                        }
                    }
                    else
                    {
                        if (elem->prev)
                            elem->prev->next = elem->next;

                        if (elem->next)
                            elem->next->prev = elem->prev;
                    }

                    elem->prev = nullptr;
                    elem->next = nullptr;

                    --_size;

                    return true;
                }
            }
            else
            {
                if (elem)
                {
                    if (_first == elem)
                    {
                        _first = elem->next;

                        if (_last == elem)
                        {
                            // so we had only one element
                            _last = _first;
                        }

                        elem->next = nullptr;

                        --_size;
                        return true;
                    }

                    auto current = _first;
                    while (current && current->next)
                    {
                        if (current->next == elem)
                        {
                            // find the element
                            // in the list
                            current->next = elem->next;

                            if (_last == elem)
                            {
                                // we have a new last element
                                _last = current;
                            }

                            elem->next = nullptr;

                            --_size;
                            return true;
                        }

                        current = current->next;
                    }
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

#endif // !__TINY_CORO_LINKED_PTR_QUEUE_HPP__