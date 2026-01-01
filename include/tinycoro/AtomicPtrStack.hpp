// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_ATOMIC_PTR_STACK_HPP
#define TINY_CORO_ATOMIC_PTR_STACK_HPP

#include <atomic>
#include <type_traits>

#include "Common.hpp"
#include "Diagnostics.hpp"
#include "LinkedUtils.hpp"
#include "CachelineAlign.hpp"

namespace tinycoro { namespace detail {

    template <concepts::Linkable NodeT>
    struct AtomicPtrStack
    {
        using value_type = std::remove_pointer_t<NodeT>;

        bool try_push(value_type* elem) noexcept
        {
            assert(elem);

#ifdef TINYCORO_DIAGNOSTICS
            TINYCORO_ASSERT(elem && elem->owner == nullptr);
#endif
            auto expected = _top.load(std::memory_order_relaxed);

            if (expected != this)
            {
#ifdef TINYCORO_DIAGNOSTICS
                // The owner must be set before we start pushing the element into the list,
                // because if we set it only after a successful push, it might be too late
                // due to a small delay.
                elem->owner = this;
#endif
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
#ifdef TINYCORO_DIAGNOSTICS
                        elem->owner = nullptr;
#endif
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
            return (top == nullptr);
        }

        [[nodiscard]] constexpr bool closed() const noexcept { return _top.load(std::memory_order_acquire) == this; }

    private:
        [[nodiscard]] value_type* exchange(void* desired) noexcept
        {
            auto expected = _top.load(std::memory_order_relaxed);
            if (expected && expected != this)
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

#ifdef TINYCORO_DIAGNOSTICS
                            // clean up the owners
                            auto elem = static_cast<value_type*>(expected);
                            ClearOwners(elem, this);
#endif

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
        alignas(CACHELINE_SIZE) std::atomic<void*> _top{nullptr};
    };

}} // namespace tinycoro::detail

#endif //TINY_CORO_ATOMIC_PTR_STACK_HPP