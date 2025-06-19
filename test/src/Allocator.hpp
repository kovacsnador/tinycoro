#ifndef TINY_CORO_TESTS_ALLOCATOR_HPP
#define TINY_CORO_TESTS_ALLOCATOR_HPP

#include <memory>
#include <memory_resource>
#include <concepts>

namespace tinycoro { namespace test {

    // Allocator to help boosting performance
    // during unit testing
    template <typename PromiseT, typename AllocatorT>
    struct AllocatorAdapter
    {
        [[nodiscard]] static void* operator new(size_t nbytes) { return AllocatorT::s_allocator.allocate_bytes(nbytes); }

        static void operator delete(void* ptr, size_t nbytes) noexcept { AllocatorT::s_allocator.deallocate_bytes(ptr, nbytes); }
    };

    template <typename OwnerT, std::unsigned_integral auto SIZE>
    struct Allocator
    {
        template<typename T>
        using adapter_t = AllocatorAdapter<T, Allocator>;

        template<typename, typename>
        friend struct AllocatorAdapter;

        void release() noexcept { s_spr.release(); }

    private:
        static inline std::unique_ptr<std::byte[]>               s_buffer = std::make_unique<std::byte[]>(SIZE);
        static inline std::pmr::monotonic_buffer_resource        s_mbr{s_buffer.get(), SIZE};
        static inline std::pmr::synchronized_pool_resource       s_spr{&s_mbr};
        static inline std::pmr::polymorphic_allocator<std::byte> s_allocator{&s_spr};
    };

}} // namespace tinycoro::test

#endif // TINY_CORO_TESTS_ALLOCATOR_HPP