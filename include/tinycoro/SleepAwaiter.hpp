// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SLEEP_AWAITER_HPP
#define TINY_CORO_SLEEP_AWAITER_HPP

#include <type_traits>
#include <chrono>

#include "Task.hpp"
#include "CancellableSuspend.hpp"
#include "AutoEvent.hpp"
#include "AllocatorAdapter.hpp"

using namespace std::chrono_literals;

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

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepUntil(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint, concepts::IsStopToken auto stopToken)
    {
        tinycoro::AutoEvent finished;

        auto cancellationToken = softClock.RegisterWithCancellation([&finished] () noexcept { finished.Set(); }, timePoint);

        std::stop_callback stopCallback{stopToken, [&cancellationToken, &finished] {
                                            if(cancellationToken.TryCancel())
                                            {
                                                finished.Set();
                                            }
                                        }};

        co_await finished;
    }

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepFor(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        co_await SleepUntil<AllocatorAdapterT>(softClock, softClock.Now() + duration, stopToken);
    }

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepFor(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration)
    {
        auto stopToken = co_await this_coro::stop_token();
        co_await SleepFor<AllocatorAdapterT>(softClock, duration, stopToken);
    }

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepUntil(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint)
    {
        auto stopToken = co_await this_coro::stop_token();
        co_await SleepUntil<AllocatorAdapterT>(softClock, timePoint, stopToken);
    }
    
    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepUntilCancellable(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint, concepts::IsStopToken auto stopToken)
    {
        co_await SleepUntil<AllocatorAdapterT>(softClock, timePoint, stopToken);

        if (stopToken.stop_requested())
        {
            co_await CancellableSuspend{};
        }
    }

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepForCancellable(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration, concepts::IsStopToken auto stopToken)
    {
        co_await SleepUntilCancellable<AllocatorAdapterT>(softClock, softClock.Now() + duration, stopToken);
    }

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepForCancellable(concepts::IsSoftClock auto& softClock, concepts::IsDuration auto duration)
    {
        auto stopToken = co_await this_coro::stop_token();
        co_await SleepForCancellable<AllocatorAdapterT>(softClock, duration, stopToken);
    }

    template<template<typename> class AllocatorAdapterT = detail::NonAllocatorAdapter>
    Task<void, AllocatorAdapterT> SleepUntilCancellable(concepts::IsSoftClock auto& softClock, concepts::IsTimePoint auto timePoint)
    {
        auto stopToken = co_await this_coro::stop_token();
        co_await SleepUntilCancellable<AllocatorAdapterT>(softClock, timePoint, stopToken);
    }

} // namespace tinycoro

#endif // TINY_CORO_SLEEP_AWAITER_HPP