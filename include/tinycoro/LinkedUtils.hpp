// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINEKD_UTILS_HPP
#define TINY_CORO_LINEKD_UTILS_HPP

namespace tinycoro { namespace detail {
    
    // helper class to indicate
    // single linkable property
    template <typename ClassT>
    struct SingleLinkable
    {
        ClassT* next{nullptr};
    };

    // helper class to indicate
    // double linkable property
    template <typename ClassT>
    struct DoubleLinkable
    {
        ClassT* next{nullptr};
        ClassT* prev{nullptr};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_LINEKD_UTILS_HPP