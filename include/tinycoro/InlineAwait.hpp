// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_INLINE_AWAIT_HPP
#define TINY_CORO_INLINE_AWAIT_HPP

#include <tuple>
#include <atomic>
#include <stop_token>

#include "Common.hpp"
#include "RunInline.hpp"

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
        struct InlineAwaitT;

        // Container specialization
        template <concepts::Iterable ContainerT>
        struct InlineAwaitT<ContainerT> : InlineAwaitBase
        {
            InlineAwaitT(ContainerT& container)
            : _container{container}
            {
            }

            [[nodiscard]] auto await_resume() { return tinycoro::RunInline(_container); }

        private:
            ContainerT& _container;
        };

        // Task specialization
        template <concepts::IsCorouitneTask... Tasks>
            requires (sizeof...(Tasks) > 0)
        struct InlineAwaitT<Tasks...> : InlineAwaitBase
        {
            InlineAwaitT(Tasks&&... tasks)
            : _tasks{std::forward_as_tuple(std::forward<Tasks>(tasks)...)}
            {
            }

            [[nodiscard]] auto await_resume()
            {
                return std::apply([&]<typename... Args>(Args&&... tasks) { return tinycoro::RunInline(std::forward<Args>(tasks)...); }, _tasks);
            }

        private:
            std::tuple<Tasks...> _tasks;
        };

        template <typename...>
        struct AnyOfInlineAwaitT;

        template <concepts::IsStopSource StopSourceT, concepts::IsCorouitneTask... Tasks>
            requires (sizeof...(Tasks) > 0)
        struct AnyOfInlineAwaitT<StopSourceT, Tasks...> : InlineAwaitBase
        {
            AnyOfInlineAwaitT(StopSourceT stopSource, Tasks&&... tasks)
            : _tasks{std::forward_as_tuple(std::forward<Tasks>(tasks)...)}
            , _stopSource{std::move(stopSource)}
            {
            }

            [[nodiscard]] auto await_resume()
            {
                return std::apply([&]<typename... Args>(Args&&... tasks) { return tinycoro::AnyOfWithStopSourceInline(std::move(_stopSource), std::forward<Args>(tasks)...); }, _tasks);
            }

        private:
            std::tuple<Tasks...> _tasks;
            StopSourceT _stopSource;
        };

        template <concepts::IsStopSource StopSourceT, concepts::Iterable ContainerT>
        struct AnyOfInlineAwaitT<StopSourceT, ContainerT> : InlineAwaitBase
        {
            AnyOfInlineAwaitT(StopSourceT stopSource, ContainerT& container)
            : _container{container}
            , _stopSource{std::move(stopSource)}
            {
            }

            [[nodiscard]] auto await_resume() { return tinycoro::AnyOfWithStopSourceInline(std::move(_stopSource), _container); }

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
        InlineAwaitT(T) -> InlineAwaitT<T>;

        template<typename... T>
        InlineAwaitT(T...) -> InlineAwaitT<T...>;


        template<typename S, typename T>
        AnyOfInlineAwaitT(S, T) -> AnyOfInlineAwaitT<S, T>;

        template<typename S, typename... T>
        AnyOfInlineAwaitT(S, T...) -> AnyOfInlineAwaitT<S, T...>;

    } // namespace detail

    template <typename... Args>
    [[nodiscard]] auto InlineAwait(Args&&... args)
    {
        return detail::InlineAwaitT{std::forward<Args>(args)...};
    }

    template <concepts::IsStopSource StopSourceT, typename... Args>
    [[nodiscard]] auto AnyOfInlineAwait(StopSourceT& stopSource, Args&&... args)
    {
        return detail::AnyOfInlineAwaitT{stopSource, std::forward<Args>(args)...};
    }

    template <concepts::IsCorouitneTask... Args>
    [[nodiscard]] auto AnyOfInlineAwait(Args&&... args)
    {
        return detail::AnyOfInlineAwaitT{std::stop_source{}, std::forward<Args>(args)...};
    }

} // namespace tinycoro

#endif // TINY_CORO_INLINE_AWAIT_HPP