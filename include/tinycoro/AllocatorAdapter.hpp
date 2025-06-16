// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_ALLOCATOR_ADAPTER_HPP
#define TINY_CORO_ALLOCATOR_ADAPTER_HPP

#include <coroutine>

namespace tinycoro
{
    namespace detail
    {
        // Empty allocator.
        // It uses the buildin allocation
        // functionality from the compiler.
        template<typename PromiseT>
        struct NonAllocatorAdapter
        {
        };

        // This is an example implementation,
        // not used in the code.
        template<typename PromiseT>
        struct ExampleAllocatorAdapter
        {
            // ensure the use of non-throwing operator-new
            [[noreturn]] static std::coroutine_handle<PromiseT> get_return_object_on_allocation_failure()
            {
                throw std::bad_alloc{};
            }

            [[nodiscard]] static void* operator new(size_t nbytes) noexcept
            {
                return std::malloc(nbytes);
            }

            static void operator delete(void* ptr, [[maybe_unused]] size_t nbytes) noexcept
            {
                std::free(ptr);
            }
        };

    } // namespace detail
    
} // namespace tinycoro


#endif // TINY_CORO_ALLOCATOR_ADAPTER_HPP