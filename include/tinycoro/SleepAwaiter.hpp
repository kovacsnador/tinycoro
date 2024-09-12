#ifndef __TINY_CORO_SLEEP_AWAITER_HPP__
#define __TINY_CORO_SLEEP_AWAITER_HPP__

#include <type_traits>
#include <chrono>
#include <future>
#include <condition_variable>

#include "AsyncCallbackAwaiter.hpp"
#include "Task.hpp"
#include "StopSourceAwaiter.hpp"

namespace tinycoro {
    namespace detail {

        template <typename>
        struct IsDurationT : std::false_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<const std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<volatile std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<const volatile std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };
        
    } // namespace detail

    namespace concepts {
        template <typename... Ts>
        concept IsDuration = detail::IsDurationT<Ts...>::value;

    } // namespace concepts

    Task<void> Sleep(concepts::IsDuration auto duration)
    {
        auto stopToken = co_await StopTokenAwaiter{};

        auto asyncCallback = [&stopToken, duration]() {
            struct NonMutex
            {
                void lock() { }
                bool try_lock() noexcept { return true; }
                void unlock() noexcept { }
            };

            NonMutex                    mtx;
            std::condition_variable_any cv;

            auto start = std::chrono::system_clock::now();

            std::unique_lock lock{mtx};
            cv.wait_for(lock, stopToken, duration, [deadLine = start + duration] { return deadLine < std::chrono::system_clock::now(); });
        };

        auto async = [](auto wrappedCallback) { return std::async(std::launch::async, wrappedCallback); };

        auto future = co_await AsyncCallbackAwaiter(async, asyncCallback);
        if (future.valid())
        {
            future.get();
        }
    }

} // namespace tinycoro

#endif //!__TINY_CORO_SLEEP_AWAITER_HPP__