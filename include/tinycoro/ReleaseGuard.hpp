// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_RELEASE_GUARD_HPP
#define TINY_CORO_RELEASE_GUARD_HPP

#include <memory>
#include <utility>

namespace tinycoro {

    template <typename DeviceT>
    class ReleaseGuard
    {
    public:
        explicit ReleaseGuard(DeviceT& d)
        : _device{std::addressof(d)}
        {
        }

        ReleaseGuard(ReleaseGuard&& other) noexcept
        : _device{std::exchange(other._device, nullptr)}
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
            if (_device)
            {
                _device->Release();
                _device = nullptr;
            }
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
        DeviceT* _device{nullptr};
    };

} // namespace tinycoro

#endif // TINY_CORO_RELEASE_GUARD_HPP