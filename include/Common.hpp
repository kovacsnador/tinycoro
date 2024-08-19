#ifndef __COMMON_HPP__
#define __COMMON_HPP__

#include <iostream>
#include <syncstream>

auto SyncOut(std::ostream& stream = std::cout)
{
    return std::osyncstream{ stream };
}

enum class ECoroResumeState
{
    SUSPENDED,
    PAUSED,
    DONE
};

#endif // !__COMMON_HPP__
