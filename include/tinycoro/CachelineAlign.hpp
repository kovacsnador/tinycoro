#ifndef TINY_CORO_CACHELINE_ALIGN_HPP
#define TINY_CORO_CACHELINE_ALIGN_HPP

#include <new> // std::hardware_destructive_interference_size

namespace tinycoro { namespace detail {

#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    // 64 bytes on x86-64 │ L1_CACHE_BYTES │ L1_CACHE_SHIFT │ __cacheline_aligned │ ...
    constexpr size_t hardware_constructive_interference_size = 64;
    constexpr size_t hardware_destructive_interference_size  = 64;
#endif

    // this is only an estimation....
    static constexpr size_t CACHELINE_SIZE = hardware_destructive_interference_size;

}} // namespace tinycoro::detail

#endif // TINY_CORO_CACHELINE_ALIGN_HPP