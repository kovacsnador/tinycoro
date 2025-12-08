// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_CO_WAIT_INLINE_HPP
#define TINY_CORO_CO_WAIT_INLINE_HPP

#include <tuple>
#include <atomic>
#include <stop_token>

#include "Common.hpp"
#include "WaitInline.hpp"

namespace tinycoro {
    namespace detail {

        struct InlineAwaitBase
        {
            InlineAwaitBase() = default;

            // disallow copy and move
            InlineAwaitBase(InlineAwaitBase&&) = delete;

            // The awaiter is always ready,
            // so jumping directly to await_resume.
            [[nodiscard]] constexpr bool await_ready() const noexcept { return true; }

            // this is probably never called
            [[nodiscard]] constexpr bool await_suspend([[maybe_unused]] auto hdl) const noexcept { return false; }
        };

        template <typename...>
        struct AllOfAwaitT;

        // Container specialization
        template <concepts::Iterable ContainerT>
        struct AllOfAwaitT<ContainerT> : InlineAwaitBase
        {
            AllOfAwaitT(ContainerT& container)
            : _container{container}
            {
            }

            [[nodiscard]] auto await_resume() { return tinycoro::AllOf(_container); }

        private:
            ContainerT& _container;
        };

        // Task specialization
        template <concepts::IsCorouitneTask... Tasks>
            requires (sizeof...(Tasks) > 0)
        struct AllOfAwaitT<Tasks...> : InlineAwaitBase
        {
            AllOfAwaitT(Tasks&&... tasks)
            : _tasks{std::forward_as_tuple(std::forward<Tasks>(tasks)...)}
            {
            }

            [[nodiscard]] auto await_resume()
            {
                return std::apply([&]<typename... Args>(Args&&... tasks) { return tinycoro::AllOf(std::forward<Args>(tasks)...); }, _tasks);
            }

        private:
            std::tuple<Tasks...> _tasks;
        };

        template <typename...>
        struct AnyOfAwaitT;

        template <concepts::IsStopSource StopSourceT, concepts::IsCorouitneTask... Tasks>
            requires (sizeof...(Tasks) > 0)
        struct AnyOfAwaitT<StopSourceT, Tasks...> : InlineAwaitBase
        {
            AnyOfAwaitT(StopSourceT stopSource, Tasks&&... tasks)
            : _tasks{std::forward_as_tuple(std::forward<Tasks>(tasks)...)}
            , _stopSource{std::move(stopSource)}
            {
            }

            [[nodiscard]] auto await_resume()
            {
                return std::apply([&]<typename... Args>(Args&&... tasks) { return tinycoro::AnyOf(std::move(_stopSource), std::forward<Args>(tasks)...); }, _tasks);
            }

        private:
            std::tuple<Tasks...> _tasks;
            StopSourceT _stopSource;
        };

        template <concepts::IsStopSource StopSourceT, concepts::Iterable ContainerT>
        struct AnyOfAwaitT<StopSourceT, ContainerT> : InlineAwaitBase
        {
            AnyOfAwaitT(StopSourceT stopSource, ContainerT& container)
            : _container{container}
            , _stopSource{std::move(stopSource)}
            {
            }

            [[nodiscard]] auto await_resume() { return tinycoro::AnyOf(std::move(_stopSource), _container); }

        private:
            ContainerT& _container;
            StopSourceT _stopSource;
        };

        // Deduction quide for template specializations
        //
        // Implicitly generated deduction guides mirror
        // the constructors of the primary template only,
        // not of the specializations.
        template<typename T>
        AllOfAwaitT(T) -> AllOfAwaitT<T>;

        template<typename... T>
        AllOfAwaitT(T...) -> AllOfAwaitT<T...>;


        template<typename S, typename T>
        AnyOfAwaitT(S, T) -> AnyOfAwaitT<S, T>;

        template<typename S, typename... T>
        AnyOfAwaitT(S, T...) -> AnyOfAwaitT<S, T...>;

    } // namespace detail

    template <concepts::IsCorouitneTask... Args>
        requires (sizeof...(Args) > 0)
    [[nodiscard]] auto AllOfAwait(Args&&... args)
    {
        return detail::AllOfAwaitT{std::forward<Args>(args)...};
    }

    template <concepts::Iterable ContainerT>
    [[nodiscard]] auto AllOfAwait(ContainerT&& container)
    {
        return detail::AllOfAwaitT{std::forward<ContainerT>(container)};
    }

    template <concepts::IsStopSource StopSourceT, concepts::IsCorouitneTask... Args>
        requires (sizeof...(Args) > 0)
    [[nodiscard]] auto AnyOfAwait(StopSourceT& stopSource, Args&&... args)
    {
        return detail::AnyOfAwaitT{stopSource, std::forward<Args>(args)...};
    }

    template <concepts::IsStopSource StopSourceT, concepts::Iterable ContainerT>
    [[nodiscard]] auto AnyOfAwait(StopSourceT& stopSource, ContainerT&& container)
    {
        return detail::AnyOfAwaitT{stopSource, std::forward<ContainerT>(container)};
    }

    template <concepts::IsCorouitneTask... Args>
        requires (sizeof...(Args) > 0)
    [[nodiscard]] auto AnyOfAwait(Args&&... args)
    {
        return detail::AnyOfAwaitT{std::stop_source{}, std::forward<Args>(args)...};
    }

    template <concepts::Iterable ContainerT>
    [[nodiscard]] auto AnyOfAwait(ContainerT&& container)
    {
        return detail::AnyOfAwaitT{std::stop_source{}, std::forward<ContainerT>(container)};
    }

} // namespace tinycoro

#endif // TINY_CORO_CO_WAIT_INLINE_HPP