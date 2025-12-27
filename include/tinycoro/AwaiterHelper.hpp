// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_AWAITER_HELPER_HPP
#define TINY_CORO_AWAITER_HELPER_HPP

#include "Common.hpp"

namespace tinycoro
{
    namespace detail
    {
        template<concepts::Linkable ObjT, typename MemberFuncT, typename... Args>
        inline void IterInvoke(ObjT top, MemberFuncT func, Args&&... args)
        {
            while (top != nullptr)
            {
                auto next = top->next;
                std::invoke(func, *top, std::forward<Args>(args)...);
                top = next;
            }
        }

    } // namespace detail
    
} // namespace tinycoro


#endif // TINY_CORO_AWAITER_HELPER_HPP