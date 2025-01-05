#ifndef __TINY_CORO_UNBUFFERED_CHANNEL_HPP__
#define __TINY_CORO_UNBUFFERED_CHANNEL_HPP__

#include <cassert>
#include <mutex>

#include "ChannelOpStatus.hpp"
#include "PauseHandler.hpp"
#include "LinkedPtrStack.hpp"
#include "LinkedPtrQueue.hpp"

namespace tinycoro {
    namespace detail {
        template <typename ValueT,
                  template <typename, typename, typename>
                  class PopAwaiterT,
                  template <typename, typename, typename>
                  class PushAwaiterT>
        class UnbufferedChannel
        {
            friend class PopAwaiterT<UnbufferedChannel, detail::PauseCallbackEvent, ValueT>;
            friend class PushAwaiterT<UnbufferedChannel, detail::PauseCallbackEvent, ValueT>;

            using pop_awaiter_type  = PopAwaiterT<UnbufferedChannel, detail::PauseCallbackEvent, ValueT>;
            using push_awaiter_type = PushAwaiterT<UnbufferedChannel, detail::PauseCallbackEvent, ValueT>;

        public:
            // default constructor
            UnbufferedChannel() = default;

            // disable move and copy
            UnbufferedChannel(UnbufferedChannel&&) = delete;

            [[nodiscard]] auto PopWait(ValueT& val) { return pop_awaiter_type{this, detail::PauseCallbackEvent{}, val}; }

            template <typename... Args>
            [[nodiscard]] auto PushWait(Args&&... args)
            {
                return push_awaiter_type{this, detail::PauseCallbackEvent{}, false, std::forward<Args>(args)...};
            }

            template <typename... Args>
            [[nodiscard]] auto PushAndCloseWait(Args&&... args)
            {
                return push_awaiter_type{this, detail::PauseCallbackEvent{}, true, std::forward<Args>(args)...};
            }

            void Close()
            {
                std::unique_lock lock{_mtx};
                _closed = true;

                auto waiters      = _waiters.steal();
                auto pushAwaiters = _pushAwaiters.steal();
                lock.unlock();

                // notify all waiters
                _NotifyAll(waiters);

                // notify all push awaiters
                _NotifyAll(pushAwaiters);
            }

            [[nodiscard]] bool IsOpen() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return !_closed;
            }

        private:
            bool IsReady(pop_awaiter_type* awaiter) { return _IsReady(awaiter, _pushAwaiters); }

            bool Add(pop_awaiter_type* awaiter) { return _Add(awaiter, _pushAwaiters, _waiters); }

            bool IsReady(push_awaiter_type* pushAwaiter) { return _IsReady(pushAwaiter, _waiters); }

            bool Add(push_awaiter_type* pushAwaiter) { return _Add(pushAwaiter, _waiters, _pushAwaiters); }

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

            void _NotifyAll(auto* awaiter)
            {
                // Notify all waiters
                while (awaiter)
                {
                    auto next = awaiter->next;
                    awaiter->Notify();
                    awaiter = next;
                }
            }

            mutable std::mutex _mtx;

            // using Queue to maintain order of pop.
            LinkedPtrQueue<pop_awaiter_type> _waiters;

            // using Queue to maintain order of values.
            LinkedPtrQueue<push_awaiter_type> _pushAwaiters;

            bool _closed{false};
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class UnbufferedChannelPopAwaiter
        {
        public:
            UnbufferedChannelPopAwaiter(ChannelT* channel, EventT event, ValueT& value)
            : _channel{channel}
            , _event{std::move(event)}
            , _value{value}
            {
                static int32_t i = 0;
                _value           = ++i;

                assert(channel);
            }

            // disable move and copy
            UnbufferedChannelPopAwaiter(UnbufferedChannelPopAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                if (_channel)
                {
                    return _channel->IsReady(this);
                }
                return true;
            }

            constexpr bool await_suspend(auto parentCoro)
            {
                if (_channel)
                {
                    PutOnPause(parentCoro);
                    if (_channel->Add(this) == false)
                    {
                        // resume immediately
                        ResumeFromPause(parentCoro);
                        return false;
                    }
                    return true;
                }
                return false;
            }

            [[nodiscard]] constexpr auto await_resume() const noexcept
            {
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

            void Notify()
            {
                // detach from the channel
                _channel = nullptr;

                // Notify scheduler to put coroutine back on CPU
                _event.Notify();
            }

            template <typename T>
            void SetValue(T&& value, bool lastElement)
            {
                assert(_set == false);

                _value       = std::forward<T>(value);
                _lastElement = lastElement;
                _set         = true;
            }

            UnbufferedChannelPopAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            // Flag to check if this is the last element in the channel. (The channel is already in closed state)
            bool _lastElement{false};

            // Flag which is true if the value is set
            bool _set{false};

            ChannelT* _channel;
            EventT    _event;
            ValueT&   _value;
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class UnbufferedChannelPushAwaiter
        {
        public:
            template <typename... Args>
            UnbufferedChannelPushAwaiter(ChannelT* channel, EventT event, bool lastValue, Args&&... args)
            : _channel{channel}
            , _event{std::move(event)}
            , _value{std::forward<Args>(args)...}
            , _lastElement{lastValue}
            {
            }

            // disable move and copy
            UnbufferedChannelPushAwaiter(UnbufferedChannelPushAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                if (_channel)
                {
                    return _channel->IsReady(this);
                }
                return true;
            }

            constexpr bool await_suspend(auto parentCoro)
            {
                if (_channel)
                {
                    PutOnPause(parentCoro);
                    if (_channel->Add(this) == false)
                    {
                        // resume immediately
                        ResumeFromPause(parentCoro);
                        return false;
                    }
                    return true;
                }
                return false;
            }

            [[nodiscard]] constexpr auto await_resume() const noexcept
            {
                if (_used)
                {
                    if (_lastElement)
                    {
                        return EChannelOpStatus::LAST;
                    }
                    return EChannelOpStatus::SUCCESS;
                }
                return EChannelOpStatus::CLOSED;
            }

            [[nodiscard]] auto Value() noexcept -> std::tuple<ValueT&, bool>
            {
                _used = true;
                return {_value, _lastElement};
            }

            void Notify()
            {
                // detach from the channel
                _channel = nullptr;

                // Notify scheduler to put coroutine back on CPU
                _event.Notify();
            }

            UnbufferedChannelPushAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            ChannelT* _channel;
            EventT    _event;
            ValueT    _value;

            // Flag to check if this is the last element in the channel. (The channel is already in closed state)
            bool _lastElement{false};

            // Flag which is true if the value is set
            bool _used{false};
        };

    } // namespace detail

    template <typename ValueT>
    using UnbufferedChannel = detail::UnbufferedChannel<ValueT, detail::UnbufferedChannelPopAwaiter, detail::UnbufferedChannelPushAwaiter>;

} // namespace tinycoro

#endif //!__TINY_CORO_UNBUFFERED_CHANNEL_HPP__