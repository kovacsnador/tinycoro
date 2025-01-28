#ifndef __TINY_CORO_SLEEP_AWAITER_HPP__
#define __TINY_CORO_SLEEP_AWAITER_HPP__

#include <type_traits>
#include <chrono>
#include <future>
#include <condition_variable>

#include "Task.hpp"
#include "AsyncCallbackAwaiter.hpp"
#include "StopSourceAwaiter.hpp"
#include "CancellableSuspend.hpp"

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

        template<typename T>
        concept IsStopToken = requires(T t) { {t.stop_requested()} -> std::same_as<bool>;
                                              {t.stop_possible()} -> std::same_as<bool>; };

    } // namespace concepts

    Task<void> Sleep(concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        auto start = std::chrono::system_clock::now();

        auto asyncCallback = [&stopToken, duration, start]() {
            struct NonMutex
            {
                void lock() { }
                bool try_lock() noexcept { return true; }
                void unlock() noexcept { }
            };

            NonMutex                    mtx;
            std::condition_variable_any cv;

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

    Task<void> Sleep(concepts::IsDuration auto duration)
    {
        auto stopToken = co_await StopTokenAwaiter{};
        co_await Sleep(duration, stopToken);
    }

    Task<void> SleepCancellable(concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        co_await Sleep(duration, stopToken);
        
        if (stopToken.stop_possible() && stopToken.stop_requested())
        {
            co_await CancellableSuspend<void>{};
        }
    }

    Task<void> SleepCancellable(concepts::IsDuration auto duration)
    {
        auto stopToken = co_await StopTokenAwaiter{};
        co_await SleepCancellable(duration, stopToken);
    }

} // namespace tinycoro

#endif //!__TINY_CORO_SLEEP_AWAITER_HPP__