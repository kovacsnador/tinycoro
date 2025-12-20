// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SOFT_CLOCK_HPP
#define TINY_CORO_SOFT_CLOCK_HPP

#include <thread>
#include <chrono>
#include <map>
#include <condition_variable>
#include <functional>
#include <algorithm>
#include <stop_token>
#include <concepts>
#include <vector>
#include <memory>
#include <memory_resource>

#include "Common.hpp"

namespace tinycoro {
    namespace detail {

        // Cancellation token for SoftClock.
        // Allows cancellation of events after they have been registered.
        //
        // If the destructor is called while the token is still valid,
        // it will attempt to cancel the associated event.
        // To prevent this behavior, call Release() to invalidate the token beforehand.
        template <template <typename, typename> class ParentT, concepts::IsDuration PrecisionT>
        class SoftClockCancelToken
        {
            // Only sharedImpl_t has access to the real constructor
            // which is private.
            friend ParentT<SoftClockCancelToken, PrecisionT>::sharedImpl_t;

        public:
            using precision_t = PrecisionT;

            // support default construction
            SoftClockCancelToken() = default;

            // allow move construction
            SoftClockCancelToken(SoftClockCancelToken&& other) noexcept
            {
                std::scoped_lock lock{_mtx, other._mtx};
                _cancellationCallback       = std::move(other._cancellationCallback);
                other._cancellationCallback = nullptr;
            }

            // allow move assignment
            SoftClockCancelToken& operator=(SoftClockCancelToken&& other) noexcept
            {
               SoftClockCancelToken{std::move(other)}.swap(*this);
                return *this;
            }

            ~SoftClockCancelToken()
            {
                // if the destructor is called
                // we automatically try to cancel the event
                TryCancel();
            }

            // release the parent and the callback
            // We detach ourself from the parent
            // and clear the cancellation callback,
            // but the event is NOT cancelled
            bool Release()
            {
                std::scoped_lock lock{_mtx};
                if(_cancellationCallback)
                {
                    _cancellationCallback = nullptr;
                    return true;
                }
                return false;
            }

            // We try to cancel the event
            // and at the same time we also make a detach
            // from the parent
            //
            // After this function call, the event is guaranteed to have either
            // been cancelled successfully or already completed execution.
            bool TryCancel()
            {
                std::unique_lock lock{_mtx};
                // we need to reset the callback, because
                // calling it 2 times can lead to UB...
                auto cancelCallback = std::exchange(_cancellationCallback, nullptr);
                lock.unlock();

                if (cancelCallback)
                {
                    // return true if cancellation was success.
                    return cancelCallback();
                }
                return true;
            }

            void swap(SoftClockCancelToken& other) noexcept
            {
                if (this != std::addressof(other))
                {
                    // need to compare the addresses here,
                    // because we lock both mutexes at the same time
                    std::scoped_lock lock{_mtx, other._mtx};
                    std::swap(other._cancellationCallback, _cancellationCallback);
                }
            }

        private:
            // private constructor
            template <typename T>
                requires (!std::same_as<std::decay<T>, SoftClockCancelToken>)
            SoftClockCancelToken(T&& cb)
            : _cancellationCallback{std::forward<T>(cb)}
            {
            }

            // this is a cancellation function
            // with this callback you can cancel the timeout
            std::function<bool()> _cancellationCallback{nullptr};

            std::mutex _mtx;
        };

        template <typename CancellationTokenT, concepts::IsDuration PrecisionT>
        class SoftClockImpl : public std::enable_shared_from_this<SoftClockImpl<CancellationTokenT, PrecisionT>>
        {
            friend CancellationTokenT;

        public:
            using precision_t = PrecisionT;

            // using steady clock
            // Class std::chrono::steady_clock represents a monotonic clock.
            using clock_t     = std::chrono::steady_clock;
            using timepoint_t = std::chrono::time_point<clock_t, precision_t>;

            using callback_t  = std::function<void()>;

            using map_t = std::pmr::multimap<timepoint_t, callback_t>;

            // Default constructor
            // without stop_token support.
            SoftClockImpl()
            : _stopCallback{std::stop_token{}, [] {}} // stop_callback, does nothing
            , _events{&_pool}
            , _thread{[this](std::stop_token token) { Run(token); }}
            {
            }

            // Constructor with custom stop token
            SoftClockImpl(std::stop_token stopToken)
            : _stopCallback{std::move(stopToken),
                            [this] {
                                // register a stop_callback which is intented to trigger the RequestStop.
                                RequestStop();
                            }}
            , _events{&_pool}
            , _thread{[this](std::stop_token token) { Run(token); }}
            {
            }

            // disable copy and move
            SoftClockImpl(SoftClockImpl&&) = delete;

            ~SoftClockImpl()
            {
                // We just simply call RequestStop
                // it notifies the jthread to stop
                // and detaches all the tokens
                RequestStop();

                if (_thread.joinable())
                {
                    // Explicitly join the jthread here to ensure proper destruction order.
                    // Although jthread automatically joins in its destructor, we must ensure
                    // that the jthread is the first member to be destroyed. This is because
                    // if the jthread destructor calls join (thread still running) after other members
                    // are destroyed, it could lead to dangling references or undefined behavior.
                    //
                    // By joining here, we guarantee that the jthread has stopped before
                    // any other members are destroyed, avoiding potential race conditions
                    // or access to invalid memory.
                    _thread.join();
                }
            }

            // Register a callback with a custom duration (no cancellation possible)
            //
            // The callback must be noexcept, as it will be invoked without 
            // exception handling. Violating this may lead to undefined behavior.
            template <concepts::IsNothrowInvokeable CbT>
            void Register(CbT&& cb, concepts::IsDuration auto duration)
            {
                Register(std::forward<CbT>(cb), clock_t::now() + duration);
            }

            // Register a callback with a custom duration (no cancellation possible)
            //
            // The callback must be noexcept, as it will be invoked without 
            // exception handling. Violating this may lead to undefined behavior.template <concepts::IsNothrowInvokeable CbT>
            template <concepts::IsNothrowInvokeable CbT>
            void Register(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                auto tp = TimePointCast(timePoint);
                RegisterImpl(std::forward<CbT>(cb), tp);
            }

            // Register a callback and get cancellation token.
            //
            // The callback must be noexcept, as it will be invoked without 
            // exception handling. Violating this may lead to undefined behavior.template <concepts::IsNothrowInvokeable CbT>
            template <concepts::IsNothrowInvokeable CbT>
            [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsDuration auto duration)
            {
                return RegisterWithCancellation(std::forward<CbT>(cb), clock_t::now() + duration);
            }

            // Register a callback and get cancellation token.
            //
            // The callback must be noexcept, as it will be invoked without 
            // exception handling. Violating this may lead to undefined behavior.template <concepts::IsNothrowInvokeable CbT>
            template <concepts::IsNothrowInvokeable CbT>
            [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                auto tp   = TimePointCast(timePoint);
                auto iter = RegisterImpl(std::forward<CbT>(cb), tp);

                if (iter.has_value())
                {
                    auto wPtr = this->weak_from_this();

                    // This cancellation callback is safe even if the event is currently executing,
                    // because the std::mutex _mtx ensures proper synchronization.
                    // If we cannot remove it from the event list, it means it has already been processed.
                    return CancellationTokenT{[this, wPtr, it = iter.value(), tp] {
                        if (auto sPtr = wPtr.lock())
                        {
                            // after we get the weak_ptr
                            // we are safe to make operations on 'this' pointer
                            std::scoped_lock lock{_mtx};

                            auto begin = _events.begin();
                            if (begin != _events.end())
                            {
                                if (tp >= begin->first)
                                {
                                    // if the begin <= tp
                                    // that means that our iterator
                                    // is still a valid one
                                    _events.erase(it);
                                    return true;
                                }
                            }
                        }
                        return false;
                    }};
                }

                return {};
            }

            bool RequestStop() noexcept
            {
                // We can delegate the stop request
                // directly to the jthread
                return _thread.request_stop();
            }

            [[nodiscard]] auto StopRequested() const noexcept { return _thread.get_stop_token().stop_requested(); }

            template <typename DurationT = precision_t>
            [[nodiscard]] constexpr static auto Now() noexcept
            {
                return std::chrono::time_point_cast<DurationT>(clock_t::now());
            }

        private:
            template <typename DurationT = timepoint_t::duration>
            static constexpr auto TimePointCast(concepts::IsTimePoint auto timePoint) noexcept
            {
                return std::chrono::time_point_cast<DurationT>(timePoint);
            }

            template <concepts::IsNothrowInvokeable CbT>
            std::optional<typename map_t::iterator> RegisterImpl(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                std::unique_lock lock{_mtx};

                if (StopRequested())
                {
                    // if the stop was requested
                    // we just return an empty optional
                    return {};
                }

                if (clock_t::now() >= timePoint)
                {
                    // check if we already passed the
                    // timepoint
                    // invoke the timeout callback
                    // immediately
                    cb();
                    return {};
                }

                auto iter = _events.insert({timePoint, callback_t{std::forward<CbT>(cb)}});

                if (iter == _events.begin())
                {
                    // we puhsed an element at the beginning.
                    // so waiting time recalculation is necessary.
                    _recalcWaitingTime = true;

                    lock.unlock();

                    // notify, that we have a new event
                    // at the beginning of the list.
                    _cv.notify_one();
                }

                return iter;
            }

            // this std::stop_token comes from the jthread itself
            // and this jthread token will be triggered through the stop_callback
            // which is registered in the constructor
            void Run(std::stop_token jthreadStopToken)
            {
                for (;;)
                {
                    std::unique_lock lock{_mtx};
                    if (_events.empty())
                    {
                        // we go to sleep if the list is empty
                        // until a new event is pushed and we get notified
                        if (_cv.wait(lock, jthreadStopToken, [this] { return _events.empty() == false; }) == false)
                        {
                            // if the stop was requested
                            // we can leave the scene
                            break;
                        }
                    }

                    assert(lock.owns_lock());
                    assert(_events.empty() == false);

                    // Save the timeout in a local varaible.
                    // this is necessary in order to prevent
                    // "heap-use-after-free"
                    // because the lock is released in wait_until()
                    // right after it is invoked.
                    auto timeout = _events.begin()->first;

                    // Wait until we can invoke the first event.
                    if (_cv.wait_until(lock, jthreadStopToken, timeout, [this] { return std::exchange(_recalcWaitingTime, false); }) == false)
                    {
                        // we need this check against stop_toke here,
                        // becasue on timeout
                        // wait_until() returns also false.
                        //
                        // This is different from wait()...
                        if (jthreadStopToken.stop_requested())
                        {
                            // if the stop was requested
                            // we can leave the scene
                            break;
                        }
                    }

                    // getting the upper bound
                    // returns an iterator to the first element greater than the given key (timepoint)
                    auto upperBound = _events.upper_bound(TimePointCast(clock_t::now()));

                    for (auto it = _events.begin(); it != upperBound; ++it)
                    {
                        // iterate over the timed out events
                        // and notify the callee about that
                        //
                        // At this point we are still holding
                        // the lock, to make sure in case we have a 
                        // running cancellation, this (cancellation) will wait
                        // until the callback is called.
                        it->second();
                    }

                    // erase the event or events which were already executed.
                    _events.erase(_events.begin(), upperBound);
                }
            }

            // The flag to indicate
            // if the waiting time recalculation
            // is necessary.
            bool _recalcWaitingTime{false};

            // The stop callback to trigger jthread to stop
            std::stop_callback<std::function<void()>> _stopCallback;

            // Mutex to protect the _events container
            std::mutex _mtx;

            // conditional variable to notify if there is new events.
            std::condition_variable_any _cv;

            // syncronized pool for events
            // It is more cache friendly if we iterate
            // on the events.
            std::pmr::synchronized_pool_resource _pool;

            // Multimap is used, because multiple callbacks
            // could be registered with the same timepoint
            map_t _events;

            // the worker thread which triggers the
            // events if they timed out
            std::jthread _thread;
        };

        template <typename CancellationTokenT, concepts::IsDuration PrecisionT>
        class SoftClock
        {
        public:
            using precision_t = PrecisionT;

            using sharedImpl_t = SoftClockImpl<CancellationTokenT, precision_t>;

            using clock_t = sharedImpl_t::clock_t;
            using timepoint_t = sharedImpl_t::timepoint_t;
            using cancelToken_t = CancellationTokenT;

            // Default constructor
            SoftClock()
            : _sharedImpl{std::make_shared<sharedImpl_t>()}
            {
            }

            // Constructor with custom stop token
            SoftClock(std::stop_token stopToken)
            : _sharedImpl{std::make_shared<sharedImpl_t>(stopToken)}
            {
            }

            // Register a callback with a custom duration (no cancellation possible)
            template <concepts::IsNothrowInvokeable CbT>
            void Register(CbT&& cb, concepts::IsDuration auto duration)
            {
                _sharedImpl->Register(std::forward<CbT>(cb), duration);
            }

            // Register a callback with a custom duration (no cancellation possible)
            template <concepts::IsNothrowInvokeable CbT>
            void Register(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                _sharedImpl->Register(std::forward<CbT>(cb), timePoint);
            }

            // Register a callback and get cancellation token.
            template <concepts::IsNothrowInvokeable CbT>
            [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsDuration auto duration)
            {
                return _sharedImpl->RegisterWithCancellation(std::forward<CbT>(cb), duration);
            }

            // Register a callback and get cancellation token.
            template <concepts::IsNothrowInvokeable CbT>
            [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                return _sharedImpl->RegisterWithCancellation(std::forward<CbT>(cb), timePoint);
            }

            void RequestStop() { _sharedImpl->RequestStop(); }

            [[nodiscard]] auto StopRequested() const noexcept { return _sharedImpl->StopRequested(); }

            // This function using the std::steady_clock
            template <typename DurationT = precision_t>
            [[nodiscard]] constexpr static auto Now() noexcept
            {
                return sharedImpl_t::template Now<DurationT>();
            }

        private:
            // contains the real implementation of the clock
            std::shared_ptr<sharedImpl_t> _sharedImpl;
        };

    } // namespace detail

    template<concepts::IsDuration PrecisionT>
    using CustomSoftClockCancelToken = detail::SoftClockCancelToken<detail::SoftClock, PrecisionT>;
    template<concepts::IsDuration PrecisionT>
    using CustomSoftClock = detail::SoftClock<CustomSoftClockCancelToken<PrecisionT>, PrecisionT>;

    using SoftClockCancelToken = CustomSoftClockCancelToken<std::chrono::milliseconds>;
    using SoftClock = CustomSoftClock<std::chrono::milliseconds>;

} // namespace tinycoro

#endif // TINY_CORO_SOFT_CLOCK_HPP