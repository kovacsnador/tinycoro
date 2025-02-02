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
#include <unordered_set>
#include <vector>

#include "Common.hpp"

namespace tinycoro {
    namespace detail {

        // this is a cancellation token
        // to be able to cancel the events
        // after registration in the SoftClock
        template <template <typename> class ParentT>
        class CancellationToken
        {
            friend class ParentT<CancellationToken>;
            using parent_t = ParentT<CancellationToken>;

        public:
            // support default construction
            CancellationToken() = default;

            // allow move construction
            CancellationToken(CancellationToken&& other)
            {
                {
                    // move all the data to the new token
                    std::scoped_lock lock{other._mtx};
                    _parent               = std::exchange(other._parent, nullptr);
                    _cancellationCallback = std::move(other._cancellationCallback);
                }

                if (_parent)
                {
                    // detach "other" from the parent
                    _parent->Detach(std::addressof(other));

                    // attach this as the new token
                    _parent->Attach(this);
                }
            }

            // allow move assignment
            CancellationToken& operator=(CancellationToken&& other)
            {
                if (this != std::addressof(other))
                {
                    std::scoped_lock lock{_mtx};

                    if (_parent)
                    {
                        // self detach from parent clock
                        _parent->Detach(this);
                    }

                    {
                        // move all the data to the new token
                        std::scoped_lock otherLock{other._mtx};
                        _cancellationCallback = std::move(other._cancellationCallback);
                        _parent               = std::exchange(other._parent, nullptr);
                    }

                    if (_parent)
                    {
                        // detach "other" from the parent clock
                        _parent->Detach(std::addressof(other));

                        // register the new token
                        _parent->Attach(this);
                    }
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
                if (_parent)
                {
                    // self detach from parent
                    _parent->Detach(this);
                    _parent = nullptr;
                }
                _cancellationCallback = nullptr;
            }

            // We try to cancel the event
            // and at the same time we also make a detach
            // from the parent
            bool TryCancel()
            {
                std::scoped_lock lock{_mtx};

                if (_parent)
                {
                    // self detach from parent
                    _parent->Detach(this);
                    _parent = nullptr;
                }

                if (_cancellationCallback)
                {
                    // invoke the cancelation callback to cancel the event
                    auto cancelled        = _cancellationCallback();
                    _cancellationCallback = nullptr;

                    return cancelled;
                }
                return false;
            }

        private:
            // private constructor
            template <typename T>
            CancellationToken(T&& cb, parent_t* parent)
            : _cancellationCallback{std::forward<T>(cb)}
            , _parent{parent}
            {
                assert(_parent);

                _parent->Attach(this);
            }

            // This function will be used
            // only from the parent
            void Disable()
            {
                std::scoped_lock lock{_mtx};

                // clear the parent
                _parent = nullptr;
                // clear the cancellation callback itself
                _cancellationCallback = nullptr;
            }

            // this is a cancellation function
            // with this callback you can cancel the timeout
            std::function<bool()> _cancellationCallback;

            // pointer to the creator parent
            // where this token is registered
            parent_t* _parent{nullptr};

            std::mutex _mtx;
        };

        // This soft clock give you a cancellation token back if you register an event
        // for this reason we are waiting for all the events to be executed if we invoked a stop
        template <typename CancellationTokenT>
        class SoftClock
        {
            friend CancellationTokenT;

        public:
            using precision_t = std::chrono::milliseconds;

            // using steady clock
            // Class std::chrono::steady_clock represents a monotonic clock.
            using clock_t     = std::chrono::steady_clock;
            using timepoint_t = std::chrono::time_point<clock_t, precision_t>;

            // the minimum allowed update frequency
            static constexpr concepts::IsDuration auto s_minFrequency = 40ms;

            using callback_t = std::function<void()>;

            using map_t = std::multimap<timepoint_t, callback_t>;

            // Constructor with custom update frequency
            // the minimum frequency is defined in s_minFrequency
            template <concepts::IsDuration T = precision_t>
            SoftClock(T frequency = 100ms)
            : _frequency{std::max(s_minFrequency, std::chrono::duration_cast<precision_t>(frequency))}
            , _stopCallback{std::stop_token{}, [] {}}
            , _thread{[this](std::stop_token token) { Run(token); }}
            {
            }

            // Constructor with custom update frequency and a stop token
            // the minimum frequency is defined in s_minFrequency
            template <concepts::IsDuration T = precision_t>
            SoftClock(std::stop_token stopToken, T frequency = 100ms)
            : _frequency{std::max(s_minFrequency, std::chrono::duration_cast<precision_t>(frequency))}
            , _stopCallback{std::move(stopToken),
                            [this] {
                                // register a stop_callback which is intented to trigger the RequestStop.
                                RequestStop();
                            }}
            , _thread{[this](std::stop_token token) { Run(token); }}
            {
            }

            // disable copy and move
            SoftClock(SoftClock&&) = delete;

            ~SoftClock()
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

            // You can register a callback
            // and you get back a cancellation token
            template <concepts::IsNothrowInvokeable CbT>
            [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsDuration auto duration)
            {
                return RegisterWithCancellation(std::forward<CbT>(cb), clock_t::now() + duration);
            }

            // You can register a callback
            // and you get back a cancellation token
            template <concepts::IsNothrowInvokeable CbT>
            [[nodiscard]] CancellationTokenT RegisterWithCancellation(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                auto tp   = std::chrono::time_point_cast<typename timepoint_t::duration>(timePoint);
                auto iter = RegisterImpl(std::forward<CbT>(cb), tp);

                if (iter.has_value())
                {
                    return CancellationTokenT{[this, it = iter.value(), tp] {
                                                  std::scoped_lock lock{_mtx};
                                                  if (tp > clock_t::now())
                                                  {
                                                      // if the time is not over yet
                                                      // we can erase the event
                                                      _events.erase(it);
                                                      return true;
                                                  }
                                                  return false;
                                              },
                                              this};
                }

                return {};
            }

            void RequestStop()
            {
                decltype(_cancellationTokens) tokensTemp{};

                {
                    std::scoped_lock lock{_mtxToken};

                    {
                        std::scoped_lock lock{_mtx};

                        // We can delegate the stop request
                        // directly to the jthread
                        _thread.request_stop();
                    }

                    // swap all the tokens
                    _cancellationTokens.swap(tokensTemp);

                    for (auto& it : tokensTemp)
                    {
                        // All the current cancellation tokens
                        // need to be notified and detached/disabled
                        it->Disable();
                    }
                }

                // clear all the bounded tokens
                //_cancellationTokens.clear();

                /*std::scoped_lock lock{_mtx};
                for (auto& it : _cancellationTokens)
                {
                    // All the current cancellation tokens
                    // need to be notified and detached/disabled
                    it->Disable();
                }

                // clear all the bounded tokens
                _cancellationTokens.clear();*/
            }

            [[nodiscard]] auto Frequency() const noexcept { return _frequency; }

            [[nodiscard]] auto StopRequested() const noexcept { return _thread.get_stop_token().stop_requested(); }

            // This function using the std::steady_clock
            [[nodiscard]] constexpr static auto Now() noexcept { return clock_t::now(); };

        private:
            template <concepts::IsNothrowInvokeable CbT>
            std::optional<map_t::iterator> RegisterImpl(CbT&& cb, concepts::IsTimePoint auto timePoint)
            {
                std::unique_lock lock{_mtx};

                if (/*timePoint < clock_t::now() ||*/ StopRequested())
                {
                    // if the event timed out already,
                    // or a stop was requested
                    // we just return an empty optional
                    return {};
                }

                auto iter = _events.insert({timePoint, callback_t{std::forward<CbT>(cb)}});
                lock.unlock();

                // notify that we have new event in the list
                _cv.notify_one();

                return iter;
            }

            // this std::stop_token comes from the jthread itself
            // and this jthread token will be triggered through the stop_callback
            // which is registered in the constructor
            void Run(std::stop_token jthreadStopToken)
            {
                // get the first/start timepoint
                auto timePoint = clock_t::now() + _frequency;

                // this is a temporary container
                // in which we copy the times out callbacks
                std::vector<callback_t> tempEvents;

                auto transformer = [](auto& pair) { return pair.second; };

                for (;;)
                {
                    std::unique_lock lock{_mtx};
                    if (_cv.wait_until(lock, jthreadStopToken, timePoint, [timePoint] { return timePoint <= clock_t::now(); }) == false)
                    {
                        // if the stop was requested
                        // we can leave the scene
                        break;
                    }

                    timePoint += _frequency;

                    if (_events.empty())
                    {
                        // we go to sleep if the list is empty
                        if (_cv.wait(lock, jthreadStopToken, [this] { return _events.empty() == false; }) == false)
                        {
                            // if the stop was requested
                            // we can leave the scene
                            break;
                        }

                        // update the timepoint after wakeup
                        // and start the iteration again
                        timePoint = clock_t::now() + _frequency;
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
                        // iterate over the timed out elements
                        // and notify the callee about that
                        it();
                    }

                    // don't forget to clear the temp container
                    tempEvents.clear();
                }
            }

            // Attach a cancellation token to the SoftClock
            bool Attach(CancellationTokenT* token)
            {
                if (token)
                {
                    std::scoped_lock lock{_mtxToken};
                    if (StopRequested() == false)
                    {
                        auto [_, inserted] = _cancellationTokens.insert(token);
                        return inserted;
                    }
                }
                return false;
            }

            // Detach a cancellation token from the SoftClock
            bool Detach(CancellationTokenT* token)
            {
                if (token)
                {
                    std::scoped_lock lock{_mtxToken};
                    return _cancellationTokens.erase(token);
                }
                return false;
            }

            // the minimum sleep time between 2 iterations
            precision_t _frequency;

            // The stop callback to trigger jthread to stop
            std::stop_callback<std::function<void()>> _stopCallback;

            // Mutex to protect the _events container
            // and the _cancellationTokens
            std::mutex _mtx;

            std::mutex _mtxToken;

            // conditional variable to notify if there is new events.
            std::condition_variable_any _cv;

            // Multimap is used, because multiple callbacks
            // could be registered with the same timepoint
            map_t _events;

            // stores the currently active cancellation tokens.
            std::unordered_set<CancellationTokenT*> _cancellationTokens;

            // the worker thread which triggers the
            // events if they timed out
            std::jthread _thread;
        };

    } // namespace detail

    using CancellationToken = detail::CancellationToken<detail::SoftClock>;
    using SoftClock         = detail::SoftClock<CancellationToken>;

} // namespace tinycoro

#endif //!__TINY_CORO_SOFT_CLOCK_HPP__