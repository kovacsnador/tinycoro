#ifndef __TINY_CORO_STATIC_STORAGE_HPP__
#define __TINY_CORO_STATIC_STORAGE_HPP__

#include <concepts>
#include <memory>
#include <type_traits>
#include <cassert>
#include <cstring>

#include "Common.hpp"
#include "Exception.hpp"

namespace tinycoro { namespace detail {

    template <typename InterpreterClassT, std::unsigned_integral auto SIZE, typename AlignmentT = void*>
    struct StaticStorage
    {
        constexpr static size_t size = SIZE;

        StaticStorage() = default;

        ~StaticStorage()
        {
            reset();
        }

        StaticStorage(StaticStorage&&) = default;
        StaticStorage& operator=(StaticStorage&&) = default;

        template <typename ClassT, typename... Args>
            requires std::constructible_from<ClassT, Args...> &&
                     (SIZE >= sizeof(ClassT)) && (alignof(AlignmentT) >= alignof(ClassT)) &&
                     ((std::derived_from<ClassT, InterpreterClassT> && concepts::HasVirtualDestructor<InterpreterClassT>) || std::same_as<ClassT, InterpreterClassT>)
        ClassT* Construct(Args&&... args)
        {
            if(_initialized)
            {
                throw StaticStorageException{"Storage object is already constructed. Use Assign(...)"}; 
            }

            _initialized = true;

            auto* self = GetAs<ClassT>();
            std::construct_at(self, std::forward<Args>(args)...);
            return self;
        }

        template <typename ClassT, typename Arg>
            requires std::assignable_from<ClassT&, Arg&&> &&
                     (SIZE >= sizeof(ClassT)) && (alignof(AlignmentT) >= alignof(ClassT)) &&
                     ((std::derived_from<ClassT, InterpreterClassT> && concepts::HasVirtualDestructor<InterpreterClassT>) || std::same_as<ClassT, InterpreterClassT>)
        ClassT* Assign(Arg&& arg)
        {
            if(_initialized == false)
            {
                throw StaticStorageException{"Storage object is NOT constructed yet. Use Construct(...) first."}; 
            }
            
            auto* self = GetAs<ClassT>();
            *self = std::forward<Arg>(arg);
            return self;
        }

        template<typename ClassT>
            requires std::derived_from<ClassT, InterpreterClassT>
        void Destroy()
        {
            if(_initialized)
            {
                std::destroy_at(GetAs<ClassT>());
                _initialized = false;
            }
        }

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

        void reset() { Destroy<InterpreterClassT>(); }

        auto* operator->() { return GetAs<InterpreterClassT>(); }

        const auto* operator->() const { return GetAs<InterpreterClassT>(); }

        operator bool() const noexcept { return _initialized; }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return !this->operator bool(); }

        [[nodiscard]] bool Empty() const noexcept { return !_initialized; }

    private:
        alignas(AlignmentT) uint8_t _buffer[SIZE];
        bool _initialized{false};
    };

}} // namespace tinycoro::detail

#endif //!__TINY_CORO_STATIC_STORAGE_HPP__