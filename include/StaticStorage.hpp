#ifndef __TINY_CORO_IN_PLACE_STORAGE_HPP__
#define __TINY_CORO_IN_PLACE_STORAGE_HPP__

#include <concepts>
#include <utility>
#include <memory>
#include <cstring>
#include <type_traits>

namespace tinycoro {

    template <typename InterpreterClassT, std::unsigned_integral auto SIZE, typename AlignasT = char>
        requires (SIZE >= sizeof(AlignasT))
    struct StaticStorage
    {
        StaticStorage() = default;

        template <typename ClassT, typename... Args>
            requires std::constructible_from<ClassT, Args...>
        StaticStorage([[maybe_unused]] std::type_identity<ClassT>, Args&&... args)
        : _owner{true}
        {
            std::construct_at(GetAs<ClassT>(), std::forward<Args>(args)...);
        }

        StaticStorage(StaticStorage&& other) noexcept
        : _owner{std::exchange(other._owner, false)}
        {
            std::memcpy(_buffer, other._buffer, SIZE);
        }

        StaticStorage& operator=(StaticStorage&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                Destroy();

                _owner = std::exchange(other._owner, false);
                std::memcpy(_buffer, other._buffer, SIZE);
            }

            return *this;
        }

        ~StaticStorage() { Destroy(); }

        void reset() { Destroy(); }

        auto* operator->() { return GetAs<InterpreterClassT>(); }

        const auto* operator->() const { return GetAs<InterpreterClassT>(); }

        operator bool() const noexcept { return _owner; }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return !this->operator bool(); }

    private:
        template <typename T>
            requires (sizeof(T) <= SIZE)
        T* GetAs()
        {
            return std::launder(reinterpret_cast<T*>(_buffer));
        }

        template <typename T>
            requires (sizeof(T) <= SIZE)
        const T* GetAs() const
        {
            return std::launder(reinterpret_cast<const T*>(_buffer));
        }

        void Destroy()
        {
            if (_owner)
            {
                std::destroy_at(GetAs<InterpreterClassT>());
                _owner = false;
            }
        }

        alignas(AlignasT) unsigned char _buffer[SIZE];

        bool _owner{false};
    };

} // namespace tinycoro
#endif // !__TINY_CORO_IN_PLACE_STORAGE_HPP__
