#ifndef __TINY_CORO_ATOMIC_PTR_STACK_HPP__
#define __TINY_CORO_ATOMIC_PTR_STACK_HPP__

#include <atomic>
#include <type_traits>

#include "Common.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
    struct AtomicPtrStack
    {
        using value_type = std::remove_pointer_t<NodeT>;

        bool try_push(value_type* elem) noexcept
        {
            auto expected = _top.load(std::memory_order_relaxed);

            if (expected != this)
            {
                // the stack is open
                elem->next = static_cast<value_type*>(expected);
                while (_top.compare_exchange_weak(expected, elem, std::memory_order_release, std::memory_order_relaxed) == false)
                {
                    if (expected == this)
                    {
                        // the stack is in closed state
                        //
                        // reset elem->next state to nullptr
                        elem->next = nullptr;
                        return false;
                    }
                    elem->next = static_cast<value_type*>(expected);
                }

                // success, we pushed the elem
                // to the top of the stack
                return true;
            }

            // the stack is closed
            return false;
        }

        [[nodiscard]] value_type* steal() noexcept { return exchange(nullptr); }

        [[nodiscard]] value_type* close() noexcept { return exchange(this); }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            auto top = _top.load(std::memory_order_acquire);
            return (top == nullptr || top == this);
        }

        [[nodiscard]] constexpr bool closed() const noexcept { return _top.load(std::memory_order_acquire) == this; }

    private:
        [[nodiscard]] value_type* exchange(void* desired) noexcept
        {
            auto expected = _top.load(std::memory_order_relaxed);
            if (expected != this)
            {
                // the stack is open
                for (;;)
                {
                    if (expected != desired)
                    {
                        if (_top.compare_exchange_weak(expected, desired, std::memory_order_release, std::memory_order_relaxed))
                        {
                            if (expected == this)
                            {
                                // the stack got closed
                                // from somewhere else
                                return nullptr;
                            }

                            // return with the top element
                            // or nullptr if the stack was empty
                            return static_cast<value_type*>(expected);
                        }
                    }
                    else
                    {
                        // to late, some other thread
                        // already did the desired exchange
                        return nullptr;
                    }
                }
            }

            // if the stack is closed
            // it also stays in that state
            return nullptr;
        }

        // nullptr -> stack is empty
        // this -> stack is closed
        // other -> stack has some content
        std::atomic<void*> _top{nullptr};
    };

}} // namespace tinycoro::detail

#endif //__TINY_CORO_ATOMIC_PTR_STACK_HPP__