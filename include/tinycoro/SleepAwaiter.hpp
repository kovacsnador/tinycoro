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
#include "Latch.hpp"

namespace tinycoro {

    namespace concepts {

        template<typename T>
        concept IsStopToken = requires(T t) { {t.stop_requested()} -> std::same_as<bool>;
                                              {t.stop_possible()} -> std::same_as<bool>; };

        template<typename T>
        concept IsSoftClock = requires(T t) { {t.Register([]{}, 1ms)}; };

    } // namespace concepts

    Task<void> Sleep(concepts::IsSoftClock auto softClock, concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        tinycoro::Latch finished{1};

        auto cancellationToken = softClock.RegisterWithCancellation([&finished]{ finished.CountDown(); }, duration);

        std::stop_callback stopCallback{stopToken, [&cancellationToken, &finished]{ cancellationToken.TryCancel(); finished.CountDown(); }};

        co_await finished;
    }

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