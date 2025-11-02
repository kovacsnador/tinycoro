// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_DETACHED_FUTURE_HPP
#define TINY_CORO_DETACHED_FUTURE_HPP

namespace tinycoro { namespace detail {

    struct [[nodiscard]] DetachedFuture
    {
        void get() const noexcept { }
    };

    template <typename FutureT>
    struct DetachedPromiseT
    {
        template <typename... Args>
        void set_value([[maybe_unused]] Args&&... args)
        {
        }

        template <typename... Args>
        void set_exception([[maybe_unused]] Args&&... args)
        {
        }

        FutureT get_future() const noexcept { return {}; }
    };

    using DetachedPromise = detail::DetachedPromiseT<DetachedFuture>;

}} // namespace tinycoro::detail

#endif // TINY_CORO_DETACHED_FUTURE_HPP