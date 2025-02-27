#ifndef __TINY_CORO_TASK_RESULT_HPP__
#define __TINY_CORO_TASK_RESULT_HPP__

#include <variant>

#include "Common.hpp"

namespace tinycoro { 

    template <typename ResultT>
    class TaskResult
    {
        using variant_t = std::variant<std::monostate, ResultT, std::exception_ptr>;

    public:
        template<typename T>
        void Set(T&& val)
        {
            _state = std::forward<T>(val);
        }

        auto& operator*()
        {
            if(auto valPtr = std::get_if<1>(_state))
            {
                return *valPtr;
            }
            
        }

    private:
        variant_t _state;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_TASK_RESULT_HPP__