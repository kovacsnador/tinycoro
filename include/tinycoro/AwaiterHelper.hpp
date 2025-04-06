#ifndef __TINY_CORO_AWAITER_HELPER_HPP__
#define __TINY_CORO_AWAITER_HELPER_HPP__

#include "Common.hpp"

namespace tinycoro
{
    namespace detail
    {
        template<concepts::Linkable ObjT, typename MemberFuncT, typename... Args>
            //requires std::invocable<MemberFuncT, Args&&...>
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


#endif //!__TINY_CORO_AWAITER_HELPER_HPP__