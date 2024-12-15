#ifndef __TINY_CORO_LOCK_GUARD_HPP__
#define __TINY_CORO_LOCK_GUARD_HPP__

#include <memory>
#include <utility>

namespace tinycoro {

    template <typename DeviceT>
    struct LockGuard
    {
        explicit LockGuard(DeviceT& d)
        : _device{std::addressof(d)}
        {
        }

        LockGuard(LockGuard&& other)
        : _device{std::exchange(other._device, nullptr)}
        {
        }

        ~LockGuard()
        {
            if (_device)
            {
                _device->Release();
            }
        }

    private:
        DeviceT* _device{nullptr};
    };

} // namespace tinycoro

#endif //!__TINY_CORO_LOCK_GUARD_HPP__