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

            newNode->next = _top;
            _top          = newNode;
        }

        // Pops and return the poped node.
        [[nodiscard]] value_type* pop() noexcept
        {
            auto top = _top;
            if (top)
            {
                --_size;
                _top = _top->next;

                if constexpr (concepts::DoubleLinkable<NodeT>)
                {
                    if (_top)
                        _top->prev = nullptr;

                    // clean up the element
                    top->prev = nullptr;
                }

                // clean up the element
                top->next = nullptr;
            }
            return top;
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
                if (elem == _top)
                {
                    _top = elem->next;

                    if (_top)
                        _top->prev = nullptr;
                }
                else
                {
                    if (elem->next)
                    {
                        elem->next->prev = elem->prev;
                    }

                    if (elem->prev)
                    {
                        elem->prev->next = elem->next;
                    }
                }

                elem->next = nullptr;
                elem->prev = nullptr;

                --_size;
                return true;
            }
            else
            {
                if (_top == elem)
                {
                    _top = elem->next;

                    elem->next = nullptr;

                    --_size;
                    return true;
                }
                else
                {
                    auto current = _top;
                    while (current && current->next)
                    {
                        if (current->next == elem)
                        {
                            // find the element
                            // in the list
                            current->next = elem->next;

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
        value_type* _top{nullptr};
        size_t      _size{};
    };
}} // namespace tinycoro::detail

#endif // TINY_CORO_LINEKD_PTR_STACK_HPP