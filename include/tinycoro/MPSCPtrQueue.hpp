// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_MPSC_PTR_QUEUE_HPP
#define TINY_CORO_MPSC_PTR_QUEUE_HPP

#include <concepts>
#include <atomic>
#include <limits>

#include "Common.hpp"
#include "AtomicPtrStack.hpp"
#include "LinkedPtrStack.hpp"

namespace tinycoro
{
    namespace detail
    {
        template<concepts::Linkable T>
        struct MPSCPtrQueue
        {
            using value_type = std::remove_pointer_t<T>;
            using value_ptr = std::add_pointer_t<value_type>;

            // multi producer allowed
            bool try_push(value_ptr elem) noexcept
            {
                return _producerStack.try_push(elem);   
            }

            // single consumer
            [[nodiscard]] bool try_pop(value_ptr& val) noexcept
            {
                auto elem = _consumerStack.pop();
                if(elem == nullptr)
                {
                    auto producedElem = _producerStack.steal();
                    while(producedElem != nullptr)
                    {
                        auto next = std::exchange(producedElem->next, nullptr);
                        if(next == nullptr)
                        {
                            val = producedElem;
                            return true;
                        }
                             
                        _consumerStack.push(producedElem);
                        producedElem = next;
                    }
                }
                val = elem;
                return val;
            }

            [[nodiscard]] bool empty() const noexcept
            {
                return _producerStack.empty() && _consumerStack.empty();
            }

            // TODO test this
            [[nodiscard]] constexpr static size_t capacity() noexcept { return std::numeric_limits<size_t>::max(); } 

        private:
            AtomicPtrStack<value_type> _producerStack;
            LinkedPtrStack<value_type> _consumerStack;

        };
    } // namespace detail    
} // namespace tinycoro




#endif // TINY_CORO_MPSC_PTR_QUEUE_HPP
