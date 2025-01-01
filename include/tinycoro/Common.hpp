#ifndef __TINY_CORO_COMMON_HPP__
#define __TINY_CORO_COMMON_HPP__

#include <type_traits>
#include <functional>
#include <atomic>
#include <memory>
#include <cassert>

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept Iterable = requires (T) {
            typename std::decay_t<T>::iterator;
            typename std::decay_t<T>::value_type;
        };

        template <typename T>
        concept NonIterable = !Iterable<T>;

        template<typename T>
        concept HasVirtualDestructor = std::has_virtual_destructor_v<T>;

        template<typename T>
        concept Linkable = requires (std::remove_pointer_t<T> n) {
            { n.next } -> std::same_as<std::remove_pointer_t<T>*&>;
        };

    } // namespace concepts

    struct VoidType
    {
    };

    enum class ETaskResumeState
    {
        SUSPENDED,
        PAUSED,
        STOPPED,
        DONE
    };

    // Coroutine id type
    using cid_t = uint64_t;

} // namespace tinycoro

#endif // !__TINY_CORO_COMMON_HPP__
