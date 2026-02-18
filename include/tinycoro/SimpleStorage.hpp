// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SIMPLE_STORAGE_HPP
#define TINY_CORO_SIMPLE_STORAGE_HPP

#include <concepts>
#include <cstddef>
#include <memory>
#include <new> /* launder */

namespace tinycoro { namespace detail {

    namespace local
    {
        template<typename T, size_t SIZE, size_t ALIGN>
        concept FitInStore = (sizeof(T) <= SIZE) && (ALIGN >= alignof(T));
    }

    // This is a simple storage object.
    //
    // It stores a simple object on his internal storage (stack).
    // It does not support copy or move operation.
    template <size_t SIZE, size_t ALIGNMENT = alignof(std::max_align_t)>
    class SimpleStorage
    { 
        using Storage_t    = std::byte[SIZE];
        using Destructor_t = void (*)(SimpleStorage*);

    public:
        SimpleStorage() = default;

        // disallow copy and move
        SimpleStorage(SimpleStorage&&) = delete;

        ~SimpleStorage() { Destroy(); }

        template <local::FitInStore<SIZE, ALIGNMENT> T, typename... Args>
        void Emplace(Args&&... args)
        {
            // destroy the previous object
            // if there was any
            Destroy();

            // The storage is not initialized yet
            // We initialize it
            std::construct_at(reinterpret_cast<T*>(_buffer), std::forward<Args>(args)...);

            // Setting the corresponding destructor
            _destructor = [](auto storage) noexcept {
                auto ptr = storage->template UnsafeGet<T>();
                std::destroy_at(ptr);
            };
        }

        // Helper function to get
        // out the real object pointer
        template <local::FitInStore<SIZE, ALIGNMENT> T>
        [[nodiscard]] T* UnsafeGet() noexcept
        {
            return std::launder(reinterpret_cast<T*>(_buffer));
        }

        // We need to get this as raw data,
        // becasue std::launder is deleted for void* type
        // anyway.
        [[nodiscard]] void* RawData() noexcept { return _buffer; }

        void Destroy() noexcept
        {
            if (_destructor)
            {
                // we holding a valid object
                // in the buffer, so we need
                // to clean up
                _destructor(this);
                _destructor = nullptr;
            }
        }

        [[nodiscard]] operator bool() const noexcept { return _destructor != nullptr; }
        [[nodiscard]] constexpr bool Empty() const noexcept { return _destructor == nullptr; }

    private:
        // the underlying buffer
        // which stores the real object
        alignas(ALIGNMENT) Storage_t _buffer;

        // The dedicated destructor
        //
        // This function knows the real object type
        // and therefore is able to destroy it properly.
        Destructor_t _destructor{nullptr};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_SIMPLE_STORAGE_HPP