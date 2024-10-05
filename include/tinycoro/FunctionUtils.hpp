#ifndef __TINY_CORO_FUNCTION_UTILS_HPP__
#define __TINY_CORO_FUNCTION_UTILS_HPP__

#include <tuple>

namespace tinycoro {

    template <typename T>
    struct FunctionSignatureT;

    template <typename R, typename... Args>
    struct FunctionSignatureT<R (*)(Args...)>
    {
        using ReturnT  = R;
        using ArgListT = std::tuple<Args...>;
    };

    template <typename R, typename... Args>
    struct FunctionSignatureT<R (*)(Args...) noexcept>
    {
        using ReturnT  = R;
        using ArgListT = std::tuple<Args...>;
    };

    // Specialization for lambdas and other callable objects
    template <typename Callable>
    struct FunctionSignatureT
    {
    private:
        // Extract the return type and arguments
        template <typename R, typename C, typename... Args>
        static auto Deduce(R (C::*)(Args...) const) -> std::tuple<R, std::tuple<Args...>>;

        // Handling non-const lambdas as well
        template <typename R, typename C, typename... Args>
        static auto Deduce(R (C::*)(Args...)) -> std::tuple<R, std::tuple<Args...>>;

        using DeductionT = decltype(Deduce(&Callable::operator()));

    public:
        // Invoke the deduce helper to extract ReturnT and ArgListT
        using ReturnT  = typename std::tuple_element<0, DeductionT>::type;
        using ArgListT = typename std::tuple_element<1, DeductionT>::type;
    };

    template <typename T>
    struct FunctionSignature
    {
        using value_type = FunctionSignatureT<std::decay_t<T>>;

        using ReturnT  = typename value_type::ReturnT;
        using ArgListT = typename value_type::ArgListT;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_FUNCTION_UTILS_HPP__