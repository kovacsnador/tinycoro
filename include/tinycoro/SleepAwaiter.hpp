#ifndef __TINY_CORO_SLEEP_AWAITER_HPP__
#define __TINY_CORO_SLEEP_AWAITER_HPP__

#include <type_traits>
#include <chrono>
#include <future>
#include <condition_variable>

#include "AsyncCallbackAwaiter.hpp"
#include "CoroTask.hpp"
#include "StopSourceAwaiter.hpp"

namespace tinycoro
{
    template<typename>
    struct IsDurationT : std::false_type {};

    template<typename RepT, typename PerT>
    struct IsDurationT<std::chrono::duration<RepT, PerT>> : std::true_type {};

    template<typename RepT, typename PerT>
    struct IsDurationT<const std::chrono::duration<RepT, PerT>> : std::true_type {};

    template<typename RepT, typename PerT>
    struct IsDurationT<volatile std::chrono::duration<RepT, PerT>> : std::true_type {};

    template<typename RepT, typename PerT>
    struct IsDurationT<const volatile std::chrono::duration<RepT, PerT>> : std::true_type {};

    namespace concepts
    {
        template<typename... Ts>
        concept IsDuration = IsDurationT<Ts...>::value;

    } // namespace concepts
    

    Task<void> Sleep(concepts::IsDuration auto duration)
    {
        auto stopToken = co_await StopTokenAwaiter{};

        tinycoro::Event event;

        auto asyncCallback = [&stopToken, duration, &event] () {

            struct NonMutex
            {
                void lock() {}
                bool try_lock() noexcept { return true;}
                void unlock() noexcept {}
            };

            NonMutex mtx;
            std::condition_variable_any cv;

            auto start = std::chrono::system_clock::now();

            std::unique_lock lock{mtx};
            cv.wait_for(lock, stopToken, duration, [&start, &duration]{ return start + duration < std::chrono::system_clock::now(); });

            event.Notify(); 
        }; 

        auto future = co_await AsyncCallbackAwaiter{[&asyncCallback] { return std::async(std::launch::async, asyncCallback); }, event};
        future.get();
    }
    
} // namespace tinycoro


#endif //!__TINY_CORO_SLEEP_AWAITER_HPP__