#ifndef TINY_CORO_ANY_OBJECT_HPP
#define TINY_CORO_ANY_OBJECT_HPP

#include <memory>

namespace tinycoro { namespace detail {

    // The object holder bridge interface
    //
    // Nothing special, just has a virtual destructor
    struct IObjectBridge
    {
        virtual ~IObjectBridge() = default;
    };

    // The actual bridge which holds the object.
    // This class is only one, who knows the object type.
    template <typename ObjectT>
    struct ObjectBridgeImpl : IObjectBridge
    {
        ObjectBridgeImpl(ObjectT obj)
        : _object{std::move(obj)}
        {
        }

    private:
        ObjectT _object;
    };

    // This class is designed to hold
    // and hide any object to which supports
    // the move construction, in order to
    // extend his life time. 
    struct AnyObject
    {
        AnyObject() = default;

        template <typename ObjectT>
            requires (!std::same_as<AnyObject, ObjectT>)
        AnyObject(ObjectT&& obj)
        : _anyObj{std::make_unique<ObjectBridgeImpl<ObjectT>>(std::forward<ObjectT>(obj))}
        {
        }

        // support move operations
        AnyObject(AnyObject&&)            = default;
        AnyObject& operator=(AnyObject&&) = default;

        [[nodiscard]] constexpr operator bool() const noexcept { return _anyObj != nullptr; }

    private:
        std::unique_ptr<IObjectBridge> _anyObj;
    };
}} // namespace tinycoro::detail

#endif // TINY_CORO_ANY_OBJECT_HPP