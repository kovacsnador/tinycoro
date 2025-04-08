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
            unlock();
            _device = std::exchange(other._device, nullptr);
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

    private:
        DeviceT* _device{nullptr};
    };

} // namespace tinycoro

#endif // TINY_CORO_RELEASE_GUARD_HPP