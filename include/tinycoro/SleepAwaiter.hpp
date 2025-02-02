#ifndef __TINY_CORO_SLEEP_AWAITER_HPP__
#define __TINY_CORO_SLEEP_AWAITER_HPP__

#include <type_traits>
#include <chrono>

#include "Task.hpp"
#include "StopSourceAwaiter.hpp"
#include "CancellableSuspend.hpp"
#include "Latch.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept IsStopToken = requires (T t) {
            { t.stop_requested() } -> std::same_as<bool>;
            { t.stop_possible() } -> std::same_as<bool>;
        };

        template <typename T>
        concept IsSoftClock = requires (T t) {
            { t.RegisterWithCancellation([] () noexcept{ }, 1ms)};
        };

    } // namespace concepts

    Task<void> SleepUntil(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint, concepts::IsStopToken auto stopToken)
    {
        tinycoro::Latch finished{1};

        auto cancellationToken = softClock.RegisterWithCancellation([&finished] () noexcept { finished.CountDown(); }, timePoint);

        std::stop_callback stopCallback{stopToken, [&cancellationToken, &finished] {
                                            cancellationToken.TryCancel();
                                            finished.CountDown();
                                        }};

        co_await finished;
    }

    Task<void> SleepFor(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        co_await SleepUntil(softClock, softClock.Now() + duration, stopToken);
    }

    Task<void> SleepFor(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration)
    {
        auto stopToken = co_await StopTokenAwaiter{};
        co_await SleepFor(softClock, duration, stopToken);
    }

    Task<void> SleepUntil(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint)
    {
        auto stopToken = co_await StopTokenAwaiter{};
        co_await SleepUntil(softClock, timePoint, stopToken);
    }
    
    Task<void> SleepUntilCancellable(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint, concepts::IsStopToken auto stopToken)
    {
        co_await SleepUntil(softClock, timePoint, stopToken);

        if (stopToken.stop_possible() && stopToken.stop_requested())
        {
            co_await CancellableSuspend<void>{};
        }
    }

    Task<void> SleepForCancellable(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        co_await SleepUntilCancellable(softClock, softClock.Now() + duration, stopToken);
    }

    Task<void> SleepForCancellable(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration)
    {
        auto stopToken = co_await StopTokenAwaiter{};
        co_await SleepForCancellable(softClock, duration, stopToken);
    }

    Task<void> SleepUntilCancellable(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint)
    {
        auto stopToken = co_await StopTokenAwaiter{};
        co_await SleepUntilCancellable(softClock, timePoint, stopToken);
    }

} // namespace tinycoro

#endif //!__TINY_CORO_SLEEP_AWAITER_HPP__