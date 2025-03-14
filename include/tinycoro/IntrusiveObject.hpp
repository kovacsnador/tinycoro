#ifndef __TINY_CORO_INTRUSIVE_OBJECT_HPP__
#define __TINY_CORO_INTRUSIVE_OBJECT_HPP__

#include <atomic>
#include <concepts>

#include "IntrusivePtr.hpp"

namespace tinycoro { namespace detail {

    template <typename IntrusivePtrT>
    class IntrusiveObjectT
    {
        friend IntrusivePtrT;

    protected:
        void AddRef()
        {
            _refCount.fetch_add(1, std::memory_order_relaxed);
            _refCount.notify_all();
        }

        auto Wait(uint32_t val) const
        {
            _refCount.wait(val, std::memory_order_relaxed);
            return _refCount.load(std::memory_order_relaxed);
        }

        void ReleaseRef()
        {
            _refCount.fetch_sub(1, std::memory_order_relaxed);
            _refCount.notify_all();
        }

    public:

        auto RefCount() const { return _refCount.load(std::memory_order_relaxed); }

    private:
        std::atomic<uint32_t> _refCount{};
    };

    template <typename T>
    using IntrusiveObject = IntrusiveObjectT<IntrusivePtr<T>>;

}} // namespace tinycoro::detail

#endif //!__TINY_CORO_INTRUSIVE_OBJECT_HPP__