#ifndef TINY_CORO_UNSAFE_FUNCTION
#define TINY_CORO_UNSAFE_FUNCTION

#include <tuple>

namespace tinycoro { namespace detail {

    // the primary template
    template <typename...>
    struct UnsafeFunction;

    // This is a fast and small
    // but unsafe function wrapper.
    //
    // This function wrapper stores the arguments
    // already before invokeation.
    template <typename RetT, typename... Args>
    struct UnsafeFunction<RetT(Args...)>
    {
        using function_t = RetT (*)(Args...);

        UnsafeFunction() = default;

        template<typename... A>
        UnsafeFunction(function_t func, A&&... args)
        : _funcPtr{func}
        , _args{std::forward<A>(args)...}
        {
        }

        UnsafeFunction(std::nullptr_t)
        {
        }

        UnsafeFunction& operator=(std::nullptr_t)
        {
            _funcPtr = nullptr;
            _args = {};

            return *this;
        }
        
        inline auto operator()() const
        {
            assert(_funcPtr);
            return std::apply(_funcPtr, _args);
        }

        [[nodiscard]] operator bool() const noexcept
        {
            return _funcPtr != nullptr;
        }

        [[nodiscard]] bool operator=(std::nullptr_t) const noexcept
        {
            return _funcPtr == nullptr;
        }
 
    private:
        function_t          _funcPtr{nullptr};
        std::tuple<Args...> _args;
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_UNSAFE_FUNCTION