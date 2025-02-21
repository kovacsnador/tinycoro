#ifndef __TINY_CORO_SOFT_CLOCK_HPP__
#define __TINY_CORO_SOFT_CLOCK_HPP__

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

        // this is a cancellation token
        // to be able to cancel the events
        // after registration in the SoftClock
        template <template <typename> class ParentT>
        class CancellationToken
        {
            // Only sharedImpl_t has access to the real constructor
            friend ParentT<CancellationToken>::sharedImpl_t;

        public:
            // support default construction
            CancellationToken() = default;

            // allow move construction
            CancellationToken(CancellationToken&& other)
            {
                std::scoped_lock lock{_mtx, other._mtx};
                _cancellationCallback       = std::move(other._cancellationCallback);
                other._cancellationCallback = nullptr;
            }

            // allow move assignment
            CancellationToken& operator=(CancellationToken&& other)
            {
                if (this != std::addressof(other))
                {
                    TryCancel();

                    std::scoped_lock lock{_mtx, other._mtx};
                    _cancellationCallback       = std::move(other._cancellationCallback);
                    other._cancellationCallback = nullptr;
                }

                return *this;
            }

            ~CancellationToken()
            {
                // if the destructor is called
                // we automatically try to cancel the event
                TryCancel();
            }

            // release the parent and the callback
            // We detach ourself from the parent
            // and clear the cancellation callback,
            // but the event is NOT cancelled
            void Release()
            {
                std::scoped_lock lock{_mtx};
                _cancellationCallback = nullptr;
            }

            // We try to cancel the event
            // and at the same time we also make a detach
            // from the parent
            bool TryCancel()
            {
                if (std::scoped_lock lock{_mtx}; _cancellationCallback)
                {
                    auto result = _cancellationCallback();

                    // after invoking the cancellation callback
                    // we need to reset the callback, because
                    // calling it 2 times can lead to UB...
                    _cancellationCallback = nullptr;
                    return result;
                }
                return false;
            }

        private:
            // private constructor
            template <typename T>
                requires (!std::same_as<std::decay<T>, CancellationToken>)
            CancellationToken(T&& cb)
            : _cancellationCallback{std::forward<T>(cb)}
            {
            }

            // this is a cancellation function
            // with this callback you can cancel the timeout
            std::function<bool()> _cancellationCallback;

            std::mutex _mtx;
        };

        template <typename CancellationTokenT>
        class SoftClock
        {
        public:
            using precision_t = std::chrono::milliseconds;

            // using steady clock
            // Class std::chrono::steady_clock represents a monotonic clock.
            using clock_t     = std::chrono::steady_clock;
            using timepoint_t = std::chrono::time_point<clock_t, precision_t>;

            // the minimum allowed update frequency
            static constexpr concepts::IsDuration auto s_minFrequency = 40ms;

            class SoftClockImpl : public std::enable_shared_from_this<SoftClockImpl>
            {
                friend CancellationTokenT;

            public:
                using callback_t = std::function<void()>;

                using map_t = std::pmr::multimap<timepoint_t, callback_t>;

                // Constructor with custom update frequency
                // the minimum frequency is defined in s_minFrequency
                template <concepts::IsDuration T = precision_t>
                SoftClockImpl(T frequency = 100ms)
                : _frequency{std::max(s_minFrequency, std::chrono::duration_cast<precision_t>(frequency))}
                , _stopCallback{std::stop_token{}, [] {}}
                , _events{&_pool}
                , _thread{[this](std::stop_token token) { Run(token); }}
                {
                }

                // Constructor with custom update frequency and a stop token
                // the minimum frequency is defined in s_minFrequency
                template <concepts::IsDuration T = precision_t>
                SoftClockImpl(std::stop_token stopToken, T frequency = 100ms)
                : _frequency{std::max(s_minFrequency, std::chrono::duration_cast<precision_t>(frequency))}
                , _stopCallback{std::move(stopToken),
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
                template <concepts::IsNothrowInvokeable CbT>
                void Register(CbT&& cb, concepts::IsDuration auto duration)
                {
                    Register(std::forward<CbT>(cb), clock_t::now() + duration);
                }

                // Register a callback with a custom duration (no cancellation possible)
                template <concepts::IsNothrowInvokeable CbT>
                void Register(CbT&& cb, concepts::IsTimePoint auto timePoint)
                {
                    auto tp = std::chrono::time_point_cast<typename timepoint_t::duration>(timePoint);
                    RegisterImpl(std::forward<CbT>(cb), tp);
                }

                // Register a callback and get cancellation token.
                template <concepts::IsNothrowInvokeable CbT>
                [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsDuration auto duration)
                {
                    return RegisterWithCancellation(std::forward<CbT>(cb), clock_t::now() + duration);
                }

                // Register a callback and get cancellation token.
                template <concepts::IsNothrowInvokeable CbT>
                [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsTimePoint auto timePoint)
                {
                    auto tp   = std::chrono::time_point_cast<typename timepoint_t::duration>(timePoint);
                    auto iter = RegisterImpl(std::forward<CbT>(cb), tp);

                    if (iter.has_value())
                    {
                        auto wPtr = this->weak_from_this();

                        return CancellationTokenT{[this, wPtr, it = iter.value(), tp] {
                            if (auto sPtr = wPtr.lock())
                            {
                                // after we get the weak_ptr
                                // we are safe to make operations on 'this' pointer
                                std::scoped_lock lock{_mtx};

                                auto begin = _events.begin();
                                if (begin != _events.end())
                                {
                                    if (tp >= begin->first && StopRequested() == false)
                                    {
                                        // if the begin <= tp
                                        // that means that that out iterator 
                                        // is still valid
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

                bool RequestStop()
                {

                    std::scoped_lock lock{_mtx};

                    // We can delegate the stop request
                    // directly to the jthread
                    return _thread.request_stop();
                }

                [[nodiscard]] auto Frequency() const noexcept { return _frequency; }

                [[nodiscard]] auto StopRequested() const noexcept { return _thread.get_stop_token().stop_requested(); }

            private:
                template <concepts::IsNothrowInvokeable CbT>
                std::optional<map_t::iterator> RegisterImpl(CbT&& cb, concepts::IsTimePoint auto timePoint)
                {
                    std::unique_lock lock{_mtx};

                    if (StopRequested())
                    {
                        // if the stop was requested
                        // we just return an empty optional
                        return {};
                    }

                    auto iter = _events.insert({timePoint, callback_t{std::forward<CbT>(cb)}});
                    lock.unlock();

                    // notify, that we have a new event in the list
                    _cv.notify_one();

                    return iter;
                }

                // this std::stop_token comes from the jthread itself
                // and this jthread token will be triggered through the stop_callback
                // which is registered in the constructor
                void Run(std::stop_token jthreadStopToken)
                {
                    // this is a temporary container
                    // in which we copy the timed out callbacks
                    std::vector<callback_t> tempEvents;
                    auto transformer = [](auto& pair) { return std::move(pair.second); };

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

                        // we have some events in the map
                        auto timePoint = clock_t::now() + _frequency;
                        if (_cv.wait_until(lock, jthreadStopToken, timePoint, [timePoint] { return timePoint <= clock_t::now(); }) == false)
                        {
                            // if the stop was requested
                            // we can leave the scene
                            break;
                        }

                        // getting the upper bound
                        // returns an iterator to the first element greater than the given key (timepoint)
                        auto upperBound = _events.upper_bound(std::chrono::time_point_cast<typename timepoint_t::duration>(clock_t::now()));

                        // transform the callbacks into a tempEvents container
                        // and we can release the lock earlier
                        std::transform(_events.begin(), upperBound, std::back_inserter(tempEvents), transformer);

                        // erase the event which were already timed out.
                        _events.erase(_events.begin(), upperBound);

                        // we can now release
                        // the lock safely
                        lock.unlock();

                        for (auto& it : tempEvents)
                        {
                            // iterate over the timed out events
                            // and notify the callee about that
                            it();
                        }

                        // don't forget to clear the temp container
                        tempEvents.clear();
                    }
                }

                // the minimum sleep time between 2 iterations
                precision_t _frequency;

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

        public:
            using sharedImpl_t = SoftClockImpl;

            // Constructor with custom update frequency
            // the minimum frequency is defined in s_minFrequency
            template <concepts::IsDuration T = precision_t>
            SoftClock(T frequency = 100ms)
            : _sharedImpl{std::make_shared<sharedImpl_t>(frequency)}
            {
            }

            // Constructor with custom update frequency and a stop token
            // the minimum frequency is defined in s_minFrequency
            template <concepts::IsDuration T = precision_t>
            SoftClock(std::stop_token stopToken, T frequency = 100ms)
            : _sharedImpl{std::make_shared<sharedImpl_t>(stopToken, frequency)}
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

            [[nodiscard]] auto Frequency() const noexcept { return _sharedImpl->Frequency(); }

            [[nodiscard]] auto StopRequested() const noexcept { return _sharedImpl->StopRequested(); }

            // This function using the std::steady_clock
            template <typename DurationT = precision_t>
            [[nodiscard]] constexpr static auto Now() noexcept
            {
                return std::chrono::time_point_cast<DurationT>(clock_t::now());
            };

        private:
            // contains the real implementation of the clock
            std::shared_ptr<sharedImpl_t> _sharedImpl;
        };

    } // namespace detail

    using CancellationToken = detail::CancellationToken<detail::SoftClock>;
    using SoftClock         = detail::SoftClock<CancellationToken>;

} // namespace tinycoro

#endif //!__TINY_CORO_SOFT_CLOCK_HPP__