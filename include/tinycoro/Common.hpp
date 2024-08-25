#ifndef __TINY_CORO_COMMON_HPP__
#define __TINY_CORO_COMMON_HPP__

#include <iostream>
#include <syncstream>

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept Iterable = requires (T) {
            typename std::decay_t<T>::iterator;
            typename std::decay_t<T>::value_type;
        };

        template <typename T>
        concept NonIterable = !Iterable<T>;

    } // namespace concepts

    enum class ECoroResumeState
    {
        SUSPENDED,
        PAUSED,
        STOPPED,
        DONE
    };

} // namespace tinycoro

#endif // !__TINY_CORO_COMMON_HPP__
