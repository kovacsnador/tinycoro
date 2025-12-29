// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_RELEASE_GUARD_HPP
#define TINY_CORO_RELEASE_GUARD_HPP

#include <memory>
#include <utility>

namespace tinycoro {

    namespace detail
    {
        template <template<typename> class GuardT, typename DeviceT>
        class ReleaseGuardRelaxedImpl : public GuardT<DeviceT>
        {
        public:
            // expose the release() member function as public
            using GuardT<DeviceT>::release;

            explicit ReleaseGuardRelaxedImpl(DeviceT& device)
            : GuardT<DeviceT>{device}
            {
            }
        };
    }

    template <typename DeviceT>
    class ReleaseGuard
    {
        friend class detail::ReleaseGuardRelaxedImpl<ReleaseGuard, DeviceT>;

    public:
        explicit ReleaseGuard(DeviceT& d)
        : _device{std::addressof(d)}
        {
        }

        ReleaseGuard(ReleaseGuard&& other) noexcept
        : _device{other.release()}
        {
        }

        ReleaseGuard& operator=(ReleaseGuard&& other) noexcept
        {
            ReleaseGuard{std::move(other)}.swap(*this);
            return *this;
        }

        [[nodiscard]] bool owns_lock() const noexcept {
            return _device;
        }

        void unlock() noexcept
        {
            if (auto dev = release())
                dev->Release();
        }

        ~ReleaseGuard()
        {
            unlock();
        }

        void swap(ReleaseGuard& other) noexcept
        {
            std::swap(other._device, _device);
        }

    private:
        constexpr auto release() noexcept { return std::exchange(_device, nullptr); }

        DeviceT* _device{nullptr};
    };

    template<typename DeviceT>
    using ReleaseGuardRelaxed = detail::ReleaseGuardRelaxedImpl<ReleaseGuard, DeviceT>;

    struct ReleaseImmediately
    {
        template<typename T>
        ReleaseImmediately(ReleaseGuardRelaxed<T>&& guard)
        {
            guard.release();
        }
    };

} // namespace tinycoro

#endif // TINY_CORO_RELEASE_GUARD_HPP