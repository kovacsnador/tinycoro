// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_FINALLY_HPP
#define TINY_CORO_FINALLY_HPP

#include <utility>
#include <type_traits>
#include <concepts>

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept NoRefConstVolatile = std::same_as<T, std::remove_cvref_t<T>>;

    } // namespace concepts

    template <std::regular_invocable T>
        requires concepts::NoRefConstVolatile<T>
    class [[nodiscard]] FinalAction
    {
    public:
        using value_type = std::remove_cvref_t<T>;

        explicit FinalAction(const value_type& func)
        : _func{func}
        {
        }

        explicit FinalAction(value_type&& func)
        : _func{std::move(func)}
        {
        }

        FinalAction(FinalAction&& other) noexcept
        : _func{std::move(other._func)}
        , _own{std::exchange(other._own, false)}
        {
        }

        ~FinalAction()
        {
            if (_own)
            {
                if constexpr (requires { _func == nullptr; })
                {
                    if (_func == nullptr)
                    {
                        // e.g. we have a std::function
                        // which is empty, we just simply return. 
                        return;
                    }
                }

                _func();
            }
        }

    private:
        value_type _func{};
        bool       _own{true};
    };

    template <std::invocable T>
    [[nodiscard]] auto Finally(T&& func) noexcept
    {
        using func_t = std::remove_cvref_t<T>;
        return FinalAction<func_t>{std::forward<T>(func)};
    }

} // namespace tinycoro

#endif // TINY_CORO_FINALLY_HPP