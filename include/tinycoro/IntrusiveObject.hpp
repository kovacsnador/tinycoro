// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_INTRUSIVE_OBJECT_HPP
#define TINY_CORO_INTRUSIVE_OBJECT_HPP

#include <atomic>
#include <concepts>

#include "IntrusivePtr.hpp"

namespace tinycoro { namespace detail {

    template <typename IntrusivePtrT>
    class IntrusiveObjectT
    {
        friend IntrusivePtrT;

    protected:
        void AddRef() noexcept
        {
            _refCount.fetch_add(1, std::memory_order_relaxed);
            _refCount.notify_all();
        }

        [[nodiscard]] auto Wait(uint32_t val) const noexcept
        {
            _refCount.wait(val, std::memory_order_relaxed);
            return _refCount.load(std::memory_order_relaxed);
        }

        void ReleaseRef() noexcept
        {
            _refCount.fetch_sub(1, std::memory_order_relaxed);
            _refCount.notify_all();
        }

    public:

        [[nodiscard]] auto RefCount() const noexcept { return _refCount.load(std::memory_order_relaxed); }

    private:
        std::atomic<uint32_t> _refCount{};
    };

    template <typename T>
    using IntrusiveObject = IntrusiveObjectT<IntrusivePtr<T>>;

}} // namespace tinycoro::detail

#endif // TINY_CORO_INTRUSIVE_OBJECT_HPP