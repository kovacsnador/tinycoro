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
        using value_type = T;

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
                if constexpr (std::convertible_to<value_type, bool>)
                {
                    if (_func == false)
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
    auto Finally(T&& func)
    {
        return FinalAction<std::decay_t<T>>(std::forward<std::decay_t<T>>(func));
    }

} // namespace tinycoro

#endif // TINY_CORO_FINALLY_HPP