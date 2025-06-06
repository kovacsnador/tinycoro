// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_LINEKD_UTILS_HPP
#define TINY_CORO_LINEKD_UTILS_HPP

#include "Diagnostics.hpp"
#include "Common.hpp"

namespace tinycoro { namespace detail {

    // Helper class to indicate a singly linkable property.
    template <typename ClassT>
    struct SingleLinkable
    {
        ClassT* next{nullptr};

#ifdef TINYCORO_DIAGNOSTICS
        // Pointer to the owning list.
        //
        // This has several advantages:
        // - Improves safety and debuggability.
        // - Provides performance benefits, as we can
        //   quickly check whether the element is
        //   currently part of a list.
        // - Prevents elemenst to be mixed between 2 or more lists.
        void* owner{nullptr};
#endif
    };

    // Helper class to indicate a doubly linkable property.
    template <typename ClassT>
    struct DoubleLinkable : SingleLinkable<ClassT>
    {
        ClassT* prev{nullptr};
    };

#ifdef TINYCORO_DIAGNOSTICS

    template<concepts::Linkable NodeT>
    void ClearOwners(NodeT node, void* ownerToCompare)
    {
        while(node)
        {
            TINYCORO_ASSERT(node->owner == ownerToCompare);

            auto next = node->next;
            node->owner = nullptr;
            node = next;
        }
    }

#endif

}} // namespace tinycoro::detail

#endif // TINY_CORO_LINEKD_UTILS_HPP