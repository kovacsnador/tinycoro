#ifndef __TINY_CORO_COMMON_HPP__
#define __TINY_CORO_COMMON_HPP__

#include <iostream>
#include <syncstream>

namespace tinycoro
{
    auto SyncOut(std::ostream &stream = std::cout)
    {
        return std::osyncstream{stream};
    }

    enum class ECoroResumeState
    {
        SUSPENDED,
        PAUSED,
        DONE
    };
}

#endif // !__TINY_CORO_COMMON_HPP__
