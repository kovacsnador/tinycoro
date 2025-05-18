#ifndef TINY_CORO_INTRUSIVE_PTR_HPP
#define TINY_CORO_INTRUSIVE_PTR_HPP

#include <variant>

#include "Exception.hpp"

namespace tinycoro { namespace detail {
    
    // This class is a simple intrusive ptr with
    // a shared ownership over the contained object.
    // The first instance stores the object
    // but all other copies hold only a pointer
    // to the first value.
    template <typename T>
    struct IntrusivePtr
    {
        using value_type   = T;
        using pointer_type = T*;
        using variant_type = std::variant<T*, T>;

        IntrusivePtr() = default;

        template <typename... Args>
            requires requires (variant_type t, Args&&... a) {
                { t.template emplace<value_type>(std::forward<Args>(a)...) };
            }
        IntrusivePtr(Args&&... args)
        {
            this->emplace(std::forward<Args>(args)...);
        }

        ~IntrusivePtr() { _Release(); }

        // shallow copy
        IntrusivePtr(IntrusivePtr& other)
        : _obj{other.get()}
        {
            _AddRef();
        }

        // in this case we need other
        // as a simple reference instead of a const&,
        // becasue we need to increment the ref count.
        IntrusivePtr& operator=(IntrusivePtr& other)
        {
            if (std::holds_alternative<value_type>(_obj))
            {
                // if we hold a value
                // no copy is possible
                throw UnsafeSharedObjectException{"Shared object is already initialized"};
            }

            // release before reassign
            _Release();

            // make a shallow copy
            // we pass only the address pointer
            _obj = other.get();

            _AddRef();

            return *this;
        }

        template <typename... Args>
        void emplace(Args&&... args)
        {
            if (std::holds_alternative<value_type>(_obj))
            {
                // the value is already assigned
                throw UnsafeSharedObjectException{"Shared object is already initialized"};
            }

            _obj.template emplace<value_type>(std::forward<Args>(args)...);

            _AddRef();
        }

        const T* operator->() const
        {
            if (std::holds_alternative<value_type>(_obj))
            {
                // return the pointer of the value object
                return std::addressof(std::get<value_type>(_obj));
            }
            return std::get<value_type*>(_obj);
        }

        T* operator->()
        {
            if (std::holds_alternative<value_type>(_obj))
            {
                // return the pointer of the value object
                return std::addressof(std::get<value_type>(_obj));
            }
            return std::get<value_type*>(_obj);
        }

        auto* get() { return this->operator->(); }

        const auto* get() const { return this->operator->(); }

        operator bool() const { return this->get(); }

    private:
        void _Release() noexcept
        {
            // We can't use a simple concept here because of MSVC limitations.
            //
            // The problem is that `IntrusiveObject` has the required functions
            // (`ReleaseRef()`, `RefCount()`, `Wait(), AddRef()`) declared as `protected`, and
            // MSVC fails to recognize them in the `requires` expression.
            //
            // As a result, even though the code is valid and works with other compilers
            // like GCC or Clang, a straightforward solution using concepts doesn't compile
            // on MSVC.
            if constexpr (requires {
                              this->get()->ReleaseRef();
                              this->get()->RefCount();
                              this->get()->Wait(0);
                          })
            {
                if (auto obj = this->get())
                {
                    // the object is initialized
                    obj->ReleaseRef();

                    if (std::holds_alternative<value_type>(_obj))
                    {
                        // we hold the real object
                        // so we are waiting for the ref count
                        // to reach 0
                        auto refCount = obj->RefCount();
                        while (refCount > 0)
                        {
                            refCount = obj->Wait(refCount);
                        }
                    }
                }
            }
        }

        void _AddRef()
        {
            // We can't use a simple concept here because of MSVC limitations.
            //
            // The problem is that `IntrusiveObject` has the required functions
            // (`ReleaseRef()`, `RefCount()`, `Wait(), AddRef()`) declared as `protected`, and
            // MSVC fails to recognize them in the `requires` expression.
            //
            // As a result, even though the code is valid and works with other compilers
            // like GCC or Clang, a straightforward solution using concepts doesn't compile
            // on MSVC.
            if constexpr (requires { this->get()->AddRef(); })
            {
                if (auto obj = this->get())
                {
                    obj->AddRef();
                }
            }
        }

        variant_type _obj{nullptr};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_INTRUSIVE_PTR_HPP