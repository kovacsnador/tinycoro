#ifndef __TINY_CORO_CANCELLATION_HANDLER_HPP__
#define __TINY_CORO_CANCELLATION_HANDLER_HPP__

#include <coroutine>

namespace tinycoro
{
    struct CancellationHandler
    {
        template<typename T>
        static void MakeCancellable(auto parentCoro, T&& returnValue)
        {
            parentCoro.promise().cancellable = true;
            parentCoro.promise().return_value(std::forward<T>(returnValue));
        }

        static void MakeCancellable(auto parentCoro)
        {
            parentCoro.promise().cancellable = true;
        }
    };
    
} // namespace tinycoro


#endif //! __TINY_CORO_CANCELLATION_HANDLER_HPP__