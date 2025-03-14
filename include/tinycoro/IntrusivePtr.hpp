#ifndef __TINY_CORO_INTRUSIVE_PTR_HPP__
#define __TINY_CORO_INTRUSIVE_PTR_HPP__

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
            IntrusivePtr() = default;

            ~IntrusivePtr()
            {
                auto obj = this->get();

                if(obj)
                {
                    // the object is initialized
                    obj->ReleaseRef();

                    if(std::holds_alternative<T>(_obj))
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

            IntrusivePtr& operator=(IntrusivePtr& other)
            {
                if(std::holds_alternative<T>(_obj))
                {
                    // if we hold a value
                    // no copy is possible
                    throw UnsafeSharedObjectException{"Shared object is already initialized"};    
                }

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
                if(std::holds_alternative<T>(_obj))
                {
                    // the value is already assigned
                    throw UnsafeSharedObjectException{"Shared object is already initialized"};   
                }

                _obj.template emplace<T>(std::forward<Args>(args)...);
                this->get()->AddRef();
            }

            const T* operator->() const
            {
                if(std::holds_alternative<T>(_obj))
                {
                    // return the pointer of the value object
                    return std::addressof(std::get<T>(_obj));    
                }
                return std::get<T*>(_obj);
            }

            T* operator->()
            {
                if(std::holds_alternative<T>(_obj))
                {
                    // return the pointer of the value object
                    return std::addressof(std::get<T>(_obj));    
                }
                return std::get<T*>(_obj);
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
            std::variant<T*, T> _obj{nullptr};
        };

    } // namespace detail
    
    
} // namespace tinycoro


#endif //!__TINY_CORO_INTRUSIVE_PTR_HPP__