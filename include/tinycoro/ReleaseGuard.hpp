// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_RELEASE_GUARD_HPP
#define TINY_CORO_RELEASE_GUARD_HPP

#include <memory>
#include <utility>

namespace tinycoro {

    namespace detail {

        // Helper implementation that inherits from a Guard type and
        // re-exposes its `release()` function publicly.
        //
        // This is used to selectively relax encapsulation without making
        // `release()` public on the primary guard type.
        template <template <typename> class GuardT, typename DeviceT>
        class ReleaseGuardRelaxedImpl : public GuardT<DeviceT>
        {
        public:
            // Expose GuardT<DeviceT>::release() as public
            // (normally private in the base guard)
            using GuardT<DeviceT>::release;

            // Construct the base guard with the given device
            explicit ReleaseGuardRelaxedImpl(DeviceT& device)
            : GuardT<DeviceT>{device}
            {
            }
        };

    } // namespace detail

    template <typename DeviceT>
    class ReleaseGuard
    {
        // Allow the relaxed implementation to access private members
        friend class detail::ReleaseGuardRelaxedImpl<ReleaseGuard, DeviceT>;

    public:
        // Acquire ownership of the device (assumes the device is already locked)
        explicit ReleaseGuard(DeviceT& d)
        : _device{std::addressof(d)}
        {
        }

        // Move construction transfers ownership and releases the source
        ReleaseGuard(ReleaseGuard&& other) noexcept
        : _device{other.release()}
        {
        }

        // Move assignment via copy-and-swap idiom
        ReleaseGuard& operator=(ReleaseGuard&& other) noexcept
        {
            ReleaseGuard{std::move(other)}.swap(*this);
            return *this;
        }

        // Check whether this guard currently owns a device
        [[nodiscard]] bool owns_lock() const noexcept { return _device != nullptr; }

        // Explicitly release ownership and unlock the device
        void unlock() noexcept
        {
            if (auto dev = release())
                dev->Release();
        }

        // RAII cleanup: release the device if still owned
        ~ReleaseGuard() { unlock(); }

        // Swap ownership with another guard
        void swap(ReleaseGuard& other) noexcept { std::swap(other._device, _device); }

    private:
        // Relinquish ownership without unlocking and return the device pointer
        constexpr auto release() noexcept { return std::exchange(_device, nullptr); }

        // Pointer to the guarded device (nullptr means no ownership)
        DeviceT* _device{nullptr};
    };

    // Relaxed variant that exposes `release()` publicly
    template <typename DeviceT>
    using ReleaseGuardRelaxed = detail::ReleaseGuardRelaxedImpl<ReleaseGuard, DeviceT>;

    // Ignore the guard entirely and return the underlying device pointer.
    //
    // The guard is destroyed without performing RAII cleanup,
    // so the device remains locked and must be released manually by the caller.
    template <typename T>
    auto IgnoreGuard(ReleaseGuardRelaxed<T>&& guard) noexcept
    {
        return guard.release();
    }


} // namespace tinycoro

#endif // TINY_CORO_RELEASE_GUARD_HPP