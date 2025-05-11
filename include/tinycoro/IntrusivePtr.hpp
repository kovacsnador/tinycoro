#ifndef TINY_CORO_INTRUSIVE_PTR_HPP
#define TINY_CORO_INTRUSIVE_PTR_HPP

#include <variant>

#include "Exception.hpp"

namespace tinycoro
{
    namespace detail
    {
        // This class is a simple intrusive ptr with
        // a shared ownership over the contained object.
        // The first instance stores the object
        // but all other copies hold only a pointer
        // to the first value.
        template<typename T>
        struct IntrusivePtr
        {
            using value_type = T;
            using variant_type =  std::variant<T*, T>; 
            
            IntrusivePtr() = default;

            template<typename... Args>
                requires requires(variant_type t, Args&&... a) { { t.template emplace<value_type>(std::forward<Args>(a)...) }; }
            IntrusivePtr(Args&&... args)
            {
                this->emplace(std::forward<Args>(args)...);
            }

            ~IntrusivePtr()
            {
                _Release();
            }

            // shallow copy
            IntrusivePtr(IntrusivePtr& other)
            : _obj{other.get()}
            {
                auto obj = this->get();
                if(obj)
                {
                    obj->AddRef();
                }
            }

            // in this case we need other
            // as a simple reference instead of a const&,
            // becasue we need to increment the ref count.
            IntrusivePtr& operator=(IntrusivePtr& other)
            {
                if(std::holds_alternative<value_type>(_obj))
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
                
                auto obj = this->get();
                if(obj)
                {
                    obj->AddRef();
                }
                
                return *this;
            }

            template<typename... Args>
            void emplace(Args&&... args)
            {
                if(std::holds_alternative<value_type>(_obj))
                {
                    // the value is already assigned
                    throw UnsafeSharedObjectException{"Shared object is already initialized"};   
                }

                _obj.template emplace<value_type>(std::forward<Args>(args)...);
                this->get()->AddRef();
            }

            const T* operator->() const
            {
                if(std::holds_alternative<value_type>(_obj))
                {
                    // return the pointer of the value object
                    return std::addressof(std::get<value_type>(_obj));    
                }
                return std::get<value_type*>(_obj);
            }

            T* operator->()
            {
                if(std::holds_alternative<value_type>(_obj))
                {
                    // return the pointer of the value object
                    return std::addressof(std::get<value_type>(_obj));    
                }
                return std::get<value_type*>(_obj);
            }

            auto* get()
            {
                return this->operator->();
            }

            const auto* get() const
            {
                return this->operator->();
            }

            operator bool() const
            {
                return this->get();
            }

        private:
            void _Release() noexcept
            {
                auto obj = this->get();

                if(obj)
                {
                    // the object is initialized
                    obj->ReleaseRef();

                    if(std::holds_alternative<value_type>(_obj))
                    {
                        // we hold the real object
                        // so we are waiting for the ref count
                        // to reach 0
                        auto refCount = obj->RefCount();
                        while(refCount > 0)
                        {
                            refCount = obj->Wait(refCount);
                        }
                    }
                }
            }

            variant_type _obj{nullptr};
        };

    } // namespace detail
    
    
} // namespace tinycoro


#endif // TINY_CORO_INTRUSIVE_PTR_HPP