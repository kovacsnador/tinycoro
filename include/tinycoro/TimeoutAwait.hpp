// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_TIMEOUT_AWAIT_HPP
#define TINY_CORO_TIMEOUT_AWAIT_HPP

#include <cassert>

#include "Common.hpp"

namespace tinycoro {

    // TimeoutAwait wraps a cancellable awaiter and augments it with timeout support.
    // It registers a cancellation callback with the given clock that will request_stop()
    // if the awaited operation does not complete within the specified time.
    //
    // If the operation completes before the timeout, the clock's cancel token is cancelled.
    // Otherwise, the clock triggers cancellation, and the coroutine resumes accordingly.
    //
    // Template parameters:
    // - ClockT: clock type providing timeout scheduling and cancellation support.
    // - AwaiterT: an awaitable type satisfying concepts::IsCancellableAwait.
    // - TimeT: timeout specification, e.g., a duration or time point.
    template <typename ClockT, concepts::IsCancellableAwait AwaiterT, typename TimeT>
    struct TimeoutAwait
    {
        TimeoutAwait(ClockT& clock, AwaiterT&& awaiter, TimeT time)
        : _awaiter{awaiter}
        , _clock{clock}
        , _time{time}
        {
        }

        constexpr auto await_ready() noexcept
        {
            // delegate the call directly
            // to the awaiter.
            return _awaiter.await_ready();
        }

        constexpr auto await_suspend(auto parentCoro)
        {
            auto suspend = _awaiter.await_suspend(parentCoro);

            if (suspend)
            {
                // we suspend, and set the flag which we need to wait
                // to make sure the cancellation is done.
                //
                // We wait for this in await_resume. This is necessary becasue it
                // can happen that cancellation token stays in the softclock queue
                // and will be executed after the actual awaiter is already destroyed.
                //_awaiterSuspended.store(true, std::memory_order::release);
                _awaiterSuspended++;

                // set the event callback for the clock
                // which is intented to force resume the awaiter
                // after the timeout was reached.
                auto cancellCallback = [this]() noexcept {

                    // try to cancel the awaiter.
                    if(_awaiter.Cancel())
                    {   
                        //_awaiterCancelled.fetch_add(1, std::memory_order::release);

                        // At this point the awaiter is already
                        // cancelled, but we still force the resumption,
                        // in order to notify the awaiter.
                        if(_awaiter.Notify())
                            _awaiterCancelled.test_and_set(std::memory_order::release);
                    }

                    // cancellation is done
                    //_awaiterSuspended.store(false, std::memory_order::release);
                    _awaiterSuspended++;
                    _awaiterSuspended.notify_all();
                };

                // initialize the cancel token for the event
                _clockCancelToken = _clock.RegisterWithCancellation(cancellCallback, _time);
            }

            return suspend;
        }

        constexpr auto await_resume() noexcept
        {
            using return_t   = decltype(std::declval<AwaiterT>().await_resume());
            using optional_t = detail::TaskResult_t<return_t>;

            //_awaiterResumed.store(true, std::memory_order::release);
            
            auto suspended = _awaiterSuspended.load(std::memory_order::acquire);
 
            // We can try to cancel the clock callback.
            if(suspended && _clockCancelToken.TryCancel() == false)
            {
                // wait until the cancellation is done.
                _awaiterSuspended.wait(1);

                /*if(_awaiterCancelled.load(std::memory_order::acquire) == false)
                {
                    // ERROR!!!!!!
                    int k=0;
                    ++k;
                }

                assert(_awaiterCancelled.load(std::memory_order::acquire));*/
                
                // check if the awaiter is cancelled,
                // by the clock event. See await_suspend().
                //if(_awaiterCancelled.load(std::memory_order::acquire))
                {
                    // The cancellation succeeded,
                    // we return an empty optional
                    //return optional_t{};
                }
            }

            //assert(_clockCancelToken.TryCancel() == false);

            // We can try to cancel the clock callback.
            // We will resume the coroutine anyway here.
            //_clockCancelToken.TryCancel();

            //_awaiterSuspended.wait(true);

            // check if the awaiter is cancelled,
            // by the clock event. See await_suspend().
            if(_awaiterCancelled.test(std::memory_order::acquire))
            {
                // The cancellation succeeded,
                // we return an empty optional
                return optional_t{};
            }

            // No cancellation occured.
            // Picking up the return value...
            if constexpr (std::same_as<return_t, void>)
            {
                // Special return value handling in case of void.
                //
                // We return with an std::optional<VoidType> 
                _awaiter.await_resume();
                return optional_t{std::in_place_t{}};
            }
            else
            {
                // Wrapp the await_resume result
                // in a std::optional object,
                // and return it to the caller.
                return optional_t{_awaiter.await_resume()};
            }
        }

    private:
        // The wrapped awaiter object.
        AwaiterT& _awaiter;

        // The clock reference, which
        // schedule the "force resume" callback event depending
        // on the timeout.
        ClockT& _clock;

        // Delayed initialization.
        //
        // See real initialization in await_suspend().
        ClockT::cancelToken_t _clockCancelToken{};

        // _time could be a timepoint
        // or a duration which will be passed
        // together with the callback event to the clock.
        TimeT _time;
        
        // 0 => await ready
        // 1 => resumed normaly
        // 2 => resumed with timeout
        std::atomic<uint8_t> _awaiterSuspended{0};

        // Flag to indicate if we cancel the awaiter.
        std::atomic_flag _awaiterCancelled;

        std::atomic<bool> _awaiterResumed{false};
    };

} // namespace tinycoro

#endif // TINY_CORO_TIMEOUT_AWAIT_HPP