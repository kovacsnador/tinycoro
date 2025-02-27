#ifndef __TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP__
#define __TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP__

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
            if (node == nullptr)
            {
                return;
            }

            _size++;

            if (_root == nullptr)
            {
                // there is no root
                // initialize the root
                _root       = node;
                _root->next = nullptr;
                return;
            }

            // _node <= _root
            if ((_root->value() < node->value()) == false)
            {
                // we can put it to the beginning
                node->next = _root;
                _root      = node;
                return;
            }

            // we need to put it somewhere in the list
            // but not the first place
            auto current = _root;
            while (current->next && current->next->value() < node->value())
            {
                // we have still element to compare
                current = current->next;
            }

            // this should never fail
            assert(current);

            node->next    = current->next;
            current->next = node;
        }

        // gives the lower_bound back
        // and erase the range from the list
        value_type* lower_bound(auto valToCompare) noexcept
        {
            if (_root)
            {
                // compare with root to see if
                // valToCompare not less that root
                // if(_cmpFunc(valToCompare, _root->value()) == false)
                if ((valToCompare < _root->value()) == false)
                {
                    _size--;

                    // we have at least 2 values in the list
                    auto current = _root;
                    while (current->next && (valToCompare < current->next->value()) == false) // current->next <= valToCompare
                    {
                        _size--;
                        current = current->next;
                    }

                    // this should never fail
                    assert(current);

                    auto oldRoot  = _root;
                    _root         = current->next;
                    current->next = nullptr;
                    return oldRoot;
                }
            }
            return nullptr;
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

#endif //!__TINY_CORO_LINKED_PTR_ORDERED_LIST_HPP__