#ifndef __TINY_CORO_LINEKD_PTR_STACK_HPP__
#define __TINY_CORO_LINEKD_PTR_STACK_HPP__

#include <concepts>

namespace tinycoro { namespace detail {

    template <typename NodeT>
        requires requires (NodeT n) {
            { n.next } -> std::same_as<NodeT*&>;
        }
    struct LinkedPtrStack
    {
        void push(NodeT* newNode)
        {
            assert(newNode);

            newNode->next = _top;
            _top          = newNode;
        }

        // Pops and return the poped node.
        NodeT* pop()
        {
            auto top = _top;
            if (top)
            {
                _top = _top->next;
            }
            return top;
        }

        [[nodiscard]] constexpr NodeT* top() noexcept { return _top; }

        [[nodiscard]] constexpr bool empty() const noexcept { return !_top; }

    private:
        NodeT* _top{nullptr};
    };
}} // namespace tinycoro::detail

#endif //!__TINY_CORO_LINEKD_PTR_STACK_HPP__