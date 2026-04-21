// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CACHELINE_ALIGN_HPP
#define TINY_CORO_CACHELINE_ALIGN_HPP

#include <cstddef>

// Public override hook for projects embedding tinycoro.
// Define this before including tinycoro headers, or pass it from the build
// system/compiler command line, for example:
//   target_compile_definitions(app PRIVATE TINYCORO_CACHELINE_SIZE=128)
//   g++ ... -DTINYCORO_CACHELINE_SIZE=128
#ifndef TINYCORO_CACHELINE_SIZE
#define TINYCORO_CACHELINE_SIZE 64
#endif 

namespace tinycoro { namespace detail {
 
    // this is only an estimation....
    static constexpr size_t CACHELINE_SIZE = TINYCORO_CACHELINE_SIZE;

    static_assert(CACHELINE_SIZE > 0, "TINYCORO_CACHELINE_SIZE must be greater than zero.");
    static_assert((CACHELINE_SIZE & (CACHELINE_SIZE - 1)) == 0,
                  "TINYCORO_CACHELINE_SIZE must be a power of two.");

}} // namespace tinycoro::detail

#endif // TINY_CORO_CACHELINE_ALIGN_HPP
