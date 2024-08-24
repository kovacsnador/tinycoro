#ifndef __TINY_CORO_COMMON_HPP__
#define __TINY_CORO_COMMON_HPP__

#include <iostream>
#include <syncstream>

namespace tinycoro {

    enum class ECoroResumeState
    {
        SUSPENDED,
        PAUSED,
        STOPPED,
        DONE
    };

} // namespace tinycoro

#endif // !__TINY_CORO_COMMON_HPP__
