// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_UNBUFFERED_CHANNEL_HPP
#define TINY_CORO_UNBUFFERED_CHANNEL_HPP

#include <cassert>
#include <mutex>
#include <latch>

#include "ChannelOpStatus.hpp"
#include "PauseHandler.hpp"
#include "LinkedPtrQueue.hpp"
#include "LinkedPtrOrderedList.hpp"
#include "LinkedUtils.hpp"

namespace tinycoro {
    namespace detail {

        template <typename ValueT,
                  template <typename, typename, typename>
                  class PopAwaiterT,
                  template <typename, typename, typename>
                  class PushAwaiterT,
                  template <typename, typename>
                  class ListenerAwaiterT>
        class UnbufferedChannel
        {
            friend class PopAwaiterT<UnbufferedChannel, detail::ResumeSignalEvent, ValueT>;
            friend class PushAwaiterT<UnbufferedChannel, detail::ResumeSignalEvent, ValueT>;
            friend class ListenerAwaiterT<UnbufferedChannel, detail::ResumeSignalEvent>;

            using pop_awaiter_type      = PopAwaiterT<UnbufferedChannel, detail::ResumeSignalEvent, ValueT>;
            using push_awaiter_type     = PushAwaiterT<UnbufferedChannel, detail::ResumeSignalEvent, ValueT>;
            using listener_awaiter_type = ListenerAwaiterT<UnbufferedChannel, detail::ResumeSignalEvent>;

            using cleanupFunction_t = std::function<void(ValueT&)>;

        public:
            using value_type = ValueT;

            // default constructor
            UnbufferedChannel(cleanupFunction_t cleanupFunc = {})
            : _cleanupFunction{std::move(cleanupFunc)}
            {
            }

            // disable move and copy
            UnbufferedChannel(UnbufferedChannel&&) = delete;

            [[nodiscard]] auto PopWait(ValueT& val) { return pop_awaiter_type{*this, detail::ResumeSignalEvent{}, val}; }

            template <typename... Args>
            [[nodiscard]] auto PushWait(Args&&... args)
            {
                return push_awaiter_type{*this, detail::ResumeSignalEvent{}, _cleanupFunction, false, std::forward<Args>(args)...};
            }

            template <typename... Args>
            [[nodiscard]] auto PushAndCloseWait(Args&&... args)
            {
                return push_awaiter_type{*this, detail::ResumeSignalEvent{}, _cleanupFunction, true, std::forward<Args>(args)...};
            }

            [[nodiscard]] auto WaitForListeners(size_t listenerCount)
            {
                return listener_awaiter_type{*this, detail::ResumeSignalEvent{}, listenerCount};
            }

            /*
             * Push element to the channel from a non corouitne environment.
             * This operaton blocks the current thread until it get's notified.
             */
            template <typename... Args>
            auto Push(Args&&... args)
            {
                return _Push(false, std::forward<Args>(args)...);
            }

            /*
             * Pushing the last element to the channel from a non corouitne environment.
             * This operaton blocks the current thread until it get's notified.
             */
            template <typename... Args>
            auto PushAndClose(Args&&... args)
            {
                return _Push(true, std::forward<Args>(args)...);
            }

            void Close()
            {
                std::unique_lock lock{_mtx};
                _closed = true;

                auto waiters      = _popAwaiters.steal();
                auto pushAwaiters = _pushAwaiters.steal();
                auto listenersTop = _listenerWaiters.steal();

                lock.unlock();

                // notify all waiters
                _NotifyAll(waiters);

                // notify all push awaiters
                _NotifyAll(pushAwaiters);

                // notify all _listeners
                _NotifyAll(listenersTop);
            }

            [[nodiscard]] bool IsOpen() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return !_closed;
            }

        private:
            // Pushing element to the channel from a non corouitne environment.
            // This operaton blocks the current thread until it get's notified.
            template <typename... Args>
            auto _Push(bool lastElement, Args&&... args)
            {
                std::latch latch{1};

                // prepare a special event for notification
                detail::ResumeSignalEvent event;

                event.Set([&latch](auto) {
                    latch.count_down();
                });

                // create a custom push awaiter.
                // The channel is here unnecessary (first parameter), because non of the
                // special awaiter functions will be called except await_resume
                // to get the awaiter state.
                push_awaiter_type pushAwaiter{*this, std::move(event), _cleanupFunction, lastElement, std::forward<Args>(args)...};

                // Try to push the awaiter (with value inside) into the queue.
                if(_Add(std::addressof(pushAwaiter), _popAwaiters, _pushAwaiters))
                {
                    // wait for the flag to get notified
                    latch.wait();
                }

                // get's the awaiter state
                return pushAwaiter.await_resume();
            }

            [[nodiscard]] bool IsReady(listener_awaiter_type* waiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                if(_closed)
                {
                    // no suspend, the channel is closed
                    return true;
                }

                return waiter->value() <= _popAwaiters.size();
            }

            [[nodiscard]] bool Add(listener_awaiter_type* waiter)
            {
                const auto wantedListerenCount = waiter->value();

                std::unique_lock lock{_mtx};

                if (wantedListerenCount <= _popAwaiters.size() || _closed)
                {
                    // no suspend, we have enough listeners or the channel is already closed.
                    return false;
                }

                // insert new listener waiter into the list
                _listenerWaiters.insert(waiter);

                // suspend coroutine
                return true;
            }

            bool Cancel(listener_awaiter_type* waiter)
            {
                return _Cancel(waiter, _listenerWaiters);
            }

            bool IsReady(pop_awaiter_type* awaiter) { return _IsReady(awaiter, _pushAwaiters); }

            bool Add(pop_awaiter_type* awaiter) { return _Add(awaiter, _pushAwaiters, _popAwaiters); }

            bool Cancel(pop_awaiter_type* waiter)
            {
                return _Cancel(waiter, _popAwaiters);
            }

            bool IsReady(push_awaiter_type* pushAwaiter) { return _IsReady(pushAwaiter, _popAwaiters); }

            bool Add(push_awaiter_type* pushAwaiter) { return _Add(pushAwaiter, _popAwaiters, _pushAwaiters); }

            bool Cancel(push_awaiter_type* waiter)
            {
                return _Cancel(waiter, _pushAwaiters);
            }

            template <typename T>
            bool _IsReady(T* awaiter, auto& waiters)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    // channel is closed
                    return true;
                }

                if (auto waiter = waiters.pop())
                {
                    lock.unlock();

                    if (_ExchangeValue(waiter, awaiter))
                    {
                        // the channel is closed
                        Close();
                    }

                    // wake up the waiter
                    waiter->Notify();

                    // no suspend
                    return true;
                }

                // suspend
                return false;
            }

            template <typename T>
            bool _Add(T* awaiter, auto& waiters, auto& container)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    // channel is closed
                    return false;
                }

                if (auto waiter = waiters.pop())
                {
                    lock.unlock();

                    if (_ExchangeValue(waiter, awaiter))
                    {
                        // the channel is closed
                        Close();
                    }

                    // wake up the waiter
                    waiter->Notify();

                    // no suspend
                    return false;
                }

                container.push(awaiter);

                if constexpr (std::same_as<T, pop_awaiter_type>)
                {
                    auto listenersTop = _listenerWaiters.lower_bound(_popAwaiters.size());
                    lock.unlock();

                    // notify all if somebody waits for listerens
                    _NotifyAll(listenersTop);
                }

                // suspend needed
                return true;
            }

            // Exchanges the value between push awaiter and pop awaiter.
            // Returns true if this was the last element.
            bool _ExchangeValue(push_awaiter_type* pushAwaiter, pop_awaiter_type* awaiter)
            {
                auto [value, lastElement] = pushAwaiter->Value();

                if (lastElement)
                {
                    // close the channel if this is the last element
                    _closed = true;
                }

                // set the value
                awaiter->SetValue(std::move(value), lastElement);

                return lastElement;
            }

            bool _ExchangeValue(pop_awaiter_type* awaiter, push_awaiter_type* pushAwaiter) { return _ExchangeValue(pushAwaiter, awaiter); }

            template<typename T>
            void _NotifyAll(T* awaiter)
            {
                // Notify all waiters
                detail::IterInvoke(awaiter, &T::Notify);
            }

            inline bool _Cancel(auto awaiter, auto& list)
            {
                std::scoped_lock lock{_mtx};
                return list.erase(awaiter);
            }

            mutable std::mutex _mtx;

            // using Queue to maintain order of pop.
            LinkedPtrQueue<pop_awaiter_type> _popAwaiters;

            // using Queue to maintain order of values.
            LinkedPtrQueue<push_awaiter_type> _pushAwaiters;

            // The listerens awaiters
            LinkedPtrOrderedList<listener_awaiter_type> _listenerWaiters;

            // This is an optional cleanup function.
            // If we call close, this function will be called
            // for the rest of the elements which are stored in push_awaiters
            cleanupFunction_t _cleanupFunction{nullptr};

            bool _closed{false};
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class UnbufferedChannelPopAwaiter : public detail::SingleLinkable<UnbufferedChannelPopAwaiter<ChannelT, EventT, ValueT>>
        {
        public:
            UnbufferedChannelPopAwaiter(ChannelT& channel, EventT event, ValueT& value)
            : _channel{channel}
            , _event{std::move(event)}
            , _value{value}
            {
            }

            // disable move and copy
            UnbufferedChannelPopAwaiter(UnbufferedChannelPopAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                return _channel.IsReady(this);
            }

            constexpr bool await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                // after channel.Add never touch the _channel member again
                // it could be a dangling ref
                if (_channel.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            [[nodiscard]] constexpr auto await_resume() const noexcept
            {
                // after await_suspend channel can be a dangling reference
                // if the channel closes and calls notify
                if (_lastElement)
                {
                    return EChannelOpStatus::LAST;
                }

                if (_set)
                {
                    return EChannelOpStatus::SUCCESS;
                }

                return EChannelOpStatus::CLOSED;
            }

            bool Notify() const noexcept { return _event.Notify(ENotifyPolicy::RESUME); }
            
            bool NotifyToDestroy() const noexcept { return _event.Notify(ENotifyPolicy::DESTROY); }

            bool Cancel() noexcept { return _channel.Cancel(this); }

            template <typename T>
            void SetValue(T&& value, bool lastElement)
            {
                assert(_set == false);

                _value       = std::forward<T>(value);
                _lastElement = lastElement;
                _set         = true;
            }

        private:
            void PutOnPause(auto parentCoro) noexcept { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            // Flag to check if this is the last element in the channel. (The channel is already in closed state)
            bool _lastElement{false};

            // Flag which is true if the value is set
            bool _set{false};

            ChannelT& _channel;
            EventT    _event;
            ValueT&   _value;
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class UnbufferedChannelPushAwaiter : public detail::SingleLinkable<UnbufferedChannelPushAwaiter<ChannelT, EventT, ValueT>>
        {
            using cleanupFunction_t = std::function<void(ValueT&)>;

        public:
            template <typename... Args>
            UnbufferedChannelPushAwaiter(ChannelT& channel, EventT event, cleanupFunction_t cleanupFunc, bool lastValue, Args&&... args)
            : _channel{channel}
            , _event{std::move(event)}
            , _value{std::forward<Args>(args)...}
            , _lastElement{lastValue}
            , _cleanupFunction{std::move(cleanupFunc)}
            {
            }

            // disable move and copy
            UnbufferedChannelPushAwaiter(UnbufferedChannelPushAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                return _channel.IsReady(this);
            }

            constexpr bool await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                // after channel.Add never touch the _channel member again
                // it could be a dangling ref
                if (_channel.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            [[nodiscard]] constexpr auto await_resume() noexcept
            {
                // after await suspend channel can be a dangling reference
                // if the channel closes and calls notify 
                if (_used)
                {
                    if (_lastElement)
                    {
                        return EChannelOpStatus::LAST;
                    }
                    return EChannelOpStatus::SUCCESS;
                }

                if(_cleanupFunction)
                {
                    // If we have a cleanup function,
                    // we can perform some cleanup here,
                    // if the value was not popped from the channel
                    _cleanupFunction(_value);
                }

                return EChannelOpStatus::CLOSED;
            }

            [[nodiscard]] auto Value() noexcept -> std::tuple<ValueT&, bool>
            {
                _used = true;
                return {_value, _lastElement};
            }

            bool Notify() const noexcept { return _event.Notify(ENotifyPolicy::RESUME); }
            
            bool NotifyToDestroy() const noexcept { return _event.Notify(ENotifyPolicy::DESTROY); }

            bool Cancel() noexcept { return _channel.Cancel(this); }

        private:
            void PutOnPause(auto parentCoro) noexcept { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            ChannelT& _channel;
            EventT    _event;
            ValueT    _value;

            // Flag to check if this is the last element in the channel. (The channel is already in closed state)
            bool _lastElement{false};

            // Flag which is true if the value is in use already
            bool _used{false};

            // This is an optional cleanup function.
            // If the channel is closed and the value stuck in the push awaiter
            // we will perform this operation on the value as cleanup
            cleanupFunction_t _cleanupFunction;
        };

        template <typename ChannelT, typename EventT>
        class UnbufferedChannelListenerAwaiter : public detail::SingleLinkable<UnbufferedChannelListenerAwaiter<ChannelT, EventT>>
        {
        public:
            UnbufferedChannelListenerAwaiter(ChannelT& channel, EventT event, size_t count)
            : _channel{channel}
            , _event{std::move(event)}
            , _listenersCount{count}
            {
            }

            // disable move and copy
            UnbufferedChannelListenerAwaiter(UnbufferedChannelListenerAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept { return _channel.IsReady(this); }

            constexpr bool await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_channel.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return false;
                }
                return true;
            }

            constexpr void await_resume() const noexcept { }

            [[nodiscard]] auto value() const noexcept { return _listenersCount; }

            bool Notify() const noexcept { return _event.Notify(ENotifyPolicy::RESUME); }
            
            bool NotifyToDestroy() const noexcept { return _event.Notify(ENotifyPolicy::DESTROY); }

            bool Cancel() noexcept { return _channel.Cancel(this); }

        private:
            void PutOnPause(auto parentCoro) noexcept { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            ChannelT&    _channel;
            EventT       _event;
            const size_t _listenersCount;
        };

    } // namespace detail

    template <typename ValueT>
    using UnbufferedChannel = detail::UnbufferedChannel<ValueT,
                                                        detail::UnbufferedChannelPopAwaiter,
                                                        detail::UnbufferedChannelPushAwaiter,
                                                        detail::UnbufferedChannelListenerAwaiter>;

} // namespace tinycoro

#endif // TINY_CORO_UNBUFFERED_CHANNEL_HPP