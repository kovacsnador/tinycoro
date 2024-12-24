#ifndef __TINY_CORO_RELEASE_GUARD_HPP__
#define __TINY_CORO_RELEASE_GUARD_HPP__

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

        ReleaseGuard(ReleaseGuard&& other)
        : _device{std::exchange(other._device, nullptr)}
        {
        }

        ReleaseGuard& operator=(ReleaseGuard&& other)
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

#endif //!__TINY_CORO_RELEASE_GUARD_HPP__