// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License � see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_RESUME_CALLBACK_HPP
#define TINY_CORO_RESUME_CALLBACK_HPP

#include <tuple>
#include <type_traits>
#include <utility>

namespace tinycoro { namespace detail {

    // This class was previously implemented as a simple std::function<>, but
    // for performance reasons it has been specialized for this specific use case.
    // 
    // Stores a callable together with two bound parameters and allows
    // invoking it later with additional arguments.
    // 
    //
    // ResumeCallback owns a function object (`FuncT`) and two pre-bound arguments
    // (`FirstParamT`, `SecondParamT`). When invoked via `operator()`, the stored
    // callable is executed as:
    //
    //     func(first, second, calling_args...)
    //
    // All stored objects must be default-constructible to support safe move
    // operations and empty-state handling. A moved-from ResumeCallback is left in
    // a valid but empty state.
    //
    // This type provides value semantics (copyable and movable) and can be checked
    // for validity via the implicit `bool` conversion.
    template <typename FuncT, typename FirstParamT, typename SecondParamT>
        requires (std::is_default_constructible_v<FirstParamT> && std::is_default_constructible_v<SecondParamT>)
    struct ResumeCallback
    {
        constexpr ResumeCallback() = default;

        constexpr ResumeCallback(FuncT func, FirstParamT first = {}, SecondParamT second = {})
        : _func{std::move(func)}
        , _first{std::move(first)}
        , _second{std::move(second)}
        {
        }

        constexpr ResumeCallback(const ResumeCallback& other)
        : _func{other._func}
        , _first{other._first}
        , _second{other._second}
        {
        }

        constexpr ResumeCallback(ResumeCallback&& other) noexcept
        : _func{std::move(other._func)}
        , _first{std::move(other._first)}
        , _second{std::move(other._second)}
        {
        }

        constexpr ResumeCallback& operator=(const ResumeCallback& other)
        {
            ResumeCallback{other}.swap(*this);
            return *this;
        }

        constexpr ResumeCallback& operator=(ResumeCallback&& other) noexcept
        {
            ResumeCallback{std::move(other)}.swap(*this);
            return *this;
        }

        // needs to be guarantied noexcept
        template <typename... CallingArgsT>
        constexpr auto operator()(CallingArgsT&&... args) const noexcept
        {
            assert(_func);

            // Pass the first two parameters by value.
            // Do not change this!
            //
            // The SchedulerWorker relies on this behavior
            // in GeneratePauseResume().
            return _func(_first, _second, std::forward<CallingArgsT>(args)...);
        }

        constexpr operator bool() const noexcept { return _func; }

        constexpr void swap(ResumeCallback& other) noexcept(std::is_nothrow_swappable_v<FuncT> &&
                                                            std::is_nothrow_swappable_v<FirstParamT> &&
                                                            std::is_nothrow_swappable_v<SecondParamT>)
        {
            std::swap(_func, other._func);
            std::swap(_first, other._first);
            std::swap(_second, other._second);
        }

    private:
        FuncT _func{};

        FirstParamT  _first{};
        SecondParamT _second{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_RESUME_CALLBACK_HPP