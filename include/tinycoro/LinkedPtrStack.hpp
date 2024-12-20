#ifndef __TINY_CORO_LINEKD_PTR_STACK_HPP__
#define __TINY_CORO_LINEKD_PTR_STACK_HPP__

#include <concepts>

namespace tinycoro { namespace detail {

    template <typename NodeT>
        requires requires (std::remove_pointer_t<NodeT> n) {
            { n.next } -> std::same_as<std::remove_pointer_t<NodeT>*&>;
        }
    struct LinkedPtrStack
    {
        using value_type = std::remove_pointer_t<NodeT>;

        void push(value_type* newNode)
        {
            assert(newNode);

            ++_size;

            newNode->next = _top;
            _top          = newNode;
        }

        // Pops and return the poped node.
        [[nodiscard]] value_type* pop()
        {
            auto top = _top;
            if (top)
            {
                --_size;
                _top = _top->next;
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

    private:
        value_type* _top{nullptr};
        size_t _size{};
    };
}} // namespace tinycoro::detail

#endif //!__TINY_CORO_LINEKD_PTR_STACK_HPP__