#ifndef __TINY_CORO_EXCEPTION_HPP__
#define __TINY_CORO_EXCEPTION_HPP__

#include <stdexcept>

namespace tinycoro
{
    struct CoroHandleException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct AssociatedStateStatisfiedException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct FutureStateException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct StopRequestedException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct StaticStorageException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct SingleEventException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct BufferedChannelException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct LatchException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct StopSourceAwaiterException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    struct BarrierException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };
    
} // namespace tinycoro


#endif //!__TINY_CORO_EXCEPTION_HPP__