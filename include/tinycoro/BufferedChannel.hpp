#ifndef __TINY_CORO_BUFFERED_CHANNEL_HPP__
#define __TINY_CORO_BUFFERED_CHANNEL_HPP__

#include <mutex>
#include <queue>
#include <cassert>
#include <bitset>
#include <limits>
#include <latch>

#include "PauseHandler.hpp"
#include "Exception.hpp"
#include "ChannelOpStatus.hpp"
#include "LinkedPtrQueue.hpp"
#include "LinkedPtrStack.hpp"
#include "LinkedPtrOrderedList.hpp"

namespace tinycoro {

    namespace detail {

        template <typename ValueT,
                  template <typename, typename, typename>
                  class PopAwaiterT,
                  template <typename, typename>
                  class ListenerAwaiterT,
                  template <typename, typename, typename>
                  class PushAwaiterT,
                  template <typename>
                  class ContainerT>
        class BufferedChannel
        {
            struct Element
            {
                bool   lastElement{false};
                ValueT value;
            };

        public:
            friend class PopAwaiterT<BufferedChannel, detail::PauseCallbackEvent, ValueT>;
            friend class ListenerAwaiterT<BufferedChannel, detail::PauseCallbackEvent>;
            friend class PushAwaiterT<BufferedChannel, detail::PauseCallbackEvent, ValueT>;

            using pop_awaiter_type      = PopAwaiterT<BufferedChannel, detail::PauseCallbackEvent, ValueT>;
            using listener_awaiter_type = ListenerAwaiterT<BufferedChannel, detail::PauseCallbackEvent>;
            using push_awaiter_type     = PushAwaiterT<BufferedChannel, detail::PauseCallbackEvent, ValueT>;

            using cleanupFunction_t = std::function<void(ValueT&)>;

            // constructor
            BufferedChannel(size_t maxQueueSize = std::numeric_limits<decltype(maxQueueSize)>::max(), cleanupFunction_t cleanupFunc = {})
            : _maxQueueSize{maxQueueSize}
            , _cleanupFunction{std::move(cleanupFunc)}
            {
                if (_maxQueueSize == 0)
                {
                    throw BufferedChannelException{"BufferedChannel: queue size need to be >0."};
                }
            }

            // constructor
            BufferedChannel(cleanupFunction_t cleanupFunc)
            : _maxQueueSize{std::numeric_limits<decltype(_maxQueueSize)>::max()}
            , _cleanupFunction{std::move(cleanupFunc)}
            {
            }

            // disable copy and move
            BufferedChannel(BufferedChannel&&) = delete;

            ~BufferedChannel() { Close(); }

            [[nodiscard]] auto PopWait(ValueT& val) { return pop_awaiter_type{*this, detail::PauseCallbackEvent{}, val}; }

            [[nodiscard]] auto WaitForListeners(size_t listenerCount)
            {
                return listener_awaiter_type{*this, detail::PauseCallbackEvent{}, listenerCount};
            }

            template <typename... Args>
            [[nodiscard]] auto PushWait(Args&&... args)
            {
                return push_awaiter_type{*this, detail::PauseCallbackEvent{}, _cleanupFunction, false, std::forward<Args>(args)...};
            }

            template <typename... Args>
            [[nodiscard]] auto PushAndCloseWait(Args&&... args)
            {
                return push_awaiter_type{*this, detail::PauseCallbackEvent{}, _cleanupFunction, true, std::forward<Args>(args)...};
            }

            // This is a waiting push. If the queue is full this will block the current thread
            // until the value can be pushed into the channel.
            template <typename... Args>
            void Push(Args&&... args)
            {
                _Emplace(true, false, std::forward<Args>(args)...);
            }

            // This is a waiting push. If the queue is full this will block the current thread
            // until the value can be pushed into the channel.
            template <typename... Args>
            void PushAndClose(Args&&... args)
            {
                _Emplace(true, true, std::forward<Args>(args)...);
            }

            // Returns false if the queue is full.
            template <typename... Args>
            bool TryPush(Args&&... args)
            {
                return _Emplace(false, false, std::forward<Args>(args)...);
            }

            // Returns false if the queue is full.
            template <typename... Args>
            bool TryPushAndClose(Args&&... args)
            {
                return _Emplace(false, true, std::forward<Args>(args)...);
            }

            [[nodiscard]] bool Empty() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return _valueCollection.empty();
            }

            [[nodiscard]] auto Size() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return _valueCollection.size();
            }

            void Close()
            {
                std::unique_lock lock{_mtx};
                _closed = true;

                auto popAwaiterTop  = _popAwaiters.steal();
                auto pushAwaiterTop = _pushAwaiters.steal();

                auto listenerAwaiterTop = _listenerWaiters.steal();

                lock.unlock();

                // notify all waiters
                _NotifyAll(popAwaiterTop);
                _NotifyAll(pushAwaiterTop);
                _NotifyAll(listenerAwaiterTop);


                if(_cleanupFunction)
                {
                    // if we have a custom defined cleanup function
                    // for the remaing elements after we closed the channel
                    // we iterate over them and make the cleanup
                    for(auto& [_, value] : _valueCollection)
                    {
                        _cleanupFunction(value);
                    }
                }
            }

            [[nodiscard]] bool IsOpen() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return !_closed;
            }

            [[nodiscard]] auto MaxSize() const noexcept
            {
                return _maxQueueSize;
            }

        private:
            template <typename... Args>
            bool _Emplace(bool forceWaiting, bool close, Args&&... args)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    if (forceWaiting)
                    {
                        throw BufferedChannelException{"BufferedChannel: channel is already closed."};
                    }
                    // channel is closed and we don't need to have force waiting
                    return false;
                }

                // Is there any awaiter
                if (auto* top = _popAwaiters.pop())
                {
                    if (close)
                    {
                        _closed = true;
                    }

                    lock.unlock();

                    top->SetValue(ValueT{std::forward<Args>(args)...}, close);
                    top->Notify();

                    if (close)
                    {
                        Close();
                    }

                    // the value is passed to the next pop_awaiter
                    return true;
                }

                // check if the queue has enough place
                if (_valueCollection.size() < _maxQueueSize)
                {
                    // No awaiters
                    _valueCollection.emplace_front(close, std::forward<Args>(args)...);

                    // Still need an open channel, because pop awaiters can arrive.
                    // the value is placed in the queue
                    return true;
                }
                else if (forceWaiting)
                {
                    lock.unlock();

                    ForceWait(close, std::forward<Args>(args)...);

                    // Still need an open channel, because pop awaiters can arrive.
                    return true;
                }

                return false;
            }

            // Creates a Push awaiter on the stack and wait for finishing.
            // This is a thread blocking approuch
            template <typename... Args>
            void ForceWait(bool lastElement, Args&&... args)
            {
                // flag for event complition
                std::latch latch{1};

                // create custom event for the push_awaiter
                PauseCallbackEvent event;
                event.Set([&latch] { latch.count_down(); });

                // create custom push_awaiter for inline waiting
                // The channel is here unnecessary (first parameter), because non of the
                // special awaiter functions will be called except await_resume
                // to get the awaiter state.
                push_awaiter_type pushAwaiter{*this, std::move(event), _cleanupFunction, lastElement, std::forward<Args>(args)...};

                // try to register push_awaiter in the queue
                if (Add(std::addressof(pushAwaiter)))
                {
                    // wait for the event to be notified
                    latch.wait();
                }
            }

            [[nodiscard]] bool IsReady(pop_awaiter_type* waiter) noexcept
            {
                std::unique_lock lock{_mtx};

                auto [ready, lastElement] = _SetValue(waiter);

                // we don't need too hold the lock any more
                lock.unlock();

                if (lastElement)
                {
                    Close();
                }

                return ready;
            }

            [[nodiscard]] bool Add(pop_awaiter_type* waiter)
            {
                std::unique_lock lock{_mtx};

                auto [ready, lastElement] = _SetValue(waiter);
                auto suspend              = !ready;

                if (lastElement)
                {
                    lock.unlock();

                    Close();

                    return suspend;
                }

                if (suspend)
                {
                    _popAwaiters.push(waiter);

                    auto listenersTop = _listenerWaiters.lower_bound(_popAwaiters.size());
                    lock.unlock();

                    // notify all if somebody waits for listerens
                    _NotifyAll(listenersTop);
                }

                // susend coroutine
                return suspend;
            }

            void Cancel(pop_awaiter_type* waiter)
            {
                _Cancel(waiter, _popAwaiters);
            }

            [[nodiscard]] bool IsReady(listener_awaiter_type* waiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                if (_closed)
                {
                    // no suspend, the channel is closed
                    return true;
                }

                return waiter->value() <= _popAwaiters.size();
            }

            [[nodiscard]] bool Add(listener_awaiter_type* waiter)
            {
                const auto desiredListerenCount = waiter->value();

                std::scoped_lock lock{_mtx};

                if (desiredListerenCount <= _popAwaiters.size() || _closed)
                {
                    // no suspend if there is enough listeners or the channel is closed
                    return false;
                }

                // insert new listener waiter into the list
                _listenerWaiters.insert(waiter);

                // suspend coroutine
                return true;
            }

            void Cancel(listener_awaiter_type* waiter)
            {
                _Cancel(waiter, _listenerWaiters);
            }

            template <typename LockT>
            bool _EmplaceValue(LockT& lock, push_awaiter_type* waiter)
            {
                assert(lock.owns_lock());

                // Is there any awaiter
                if (auto* top = _popAwaiters.pop())
                {
                    auto [valueRef, lastElement] = waiter->Value();

                    if (lastElement)
                    {
                        _closed = true;
                    }

                    lock.unlock();

                    top->SetValue(std::move(valueRef), lastElement);
                    top->Notify();

                    if (lastElement)
                    {
                        Close();
                    }

                    // value is passed forward, the lock is not hold
                    return true;
                }

                // check if there place int the queue
                if (_valueCollection.size() < _maxQueueSize)
                {
                    auto [valueRef, lastElement] = waiter->Value();

                    // emplace the value in the queue
                    _valueCollection.emplace_front(lastElement, std::move(valueRef));

                    lock.unlock();

                    // value is passed forward, the lock is not hold
                    return true;
                }

                // the lock is hold
                return false;
            }

            [[nodiscard]] bool IsReady(push_awaiter_type* waiter)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    // no suspend if the channel is closed, we do nothing
                    return true;
                }

                return _EmplaceValue(lock, waiter);
            }

            [[nodiscard]] bool Add(push_awaiter_type* waiter)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    // no suspend if the channel is closed, we do nothing
                    return false;
                }

                if (_EmplaceValue(lock, waiter))
                {
                    // no suspend, the value is in queue
                    return false;
                }

                _pushAwaiters.push(waiter);

                // suspend, the push awaiter pushed into the queue
                return true;
            }

            void Cancel(push_awaiter_type* waiter)
            {
                _Cancel(waiter, _pushAwaiters);
            }

            // auto [ready, lastElement] = std::tuple<bool, bool>
            std::tuple<bool, bool> _SetValue(pop_awaiter_type* waiter)
            {
                if (_closed)
                {
                    // channel is closed, no suspend
                    return {true, false};
                }

                if (_valueCollection.empty() == false)
                {
                    auto& [lastElement, value] = _valueCollection.back();

                    if (lastElement)
                    {
                        // close the channel, last element reached
                        _closed = true;
                    }

                    auto last = lastElement;

                    waiter->SetValue(std::move(value), lastElement);
                    _valueCollection.pop_back();

                    // Get the next awaiter if there is one
                    if (auto pushAwaiter = _pushAwaiters.pop())
                    {
                        auto [value, close] = pushAwaiter->Value();

                        // emplace the value in the queue
                        _valueCollection.emplace_front(close, std::move(value));

                        // notify the push awaiter for wakeup
                        pushAwaiter->Notify();
                    }

                    // we have a value from the channel, no suspend
                    return {true, last};
                }

                // channel is empty, suspend coroutine
                return {false, false};
            }

            template<typename T>
            void _NotifyAll(T* awaiter)
            {
                // Notify all waiters
                detail::IterInvoke(awaiter, &T::Notify);
            }

            inline void _Cancel(auto awaiter, auto& list)
            {
                bool erased{false};

                {
                    std::scoped_lock lock{_mtx};
                    erased = list.erase(awaiter);
                }
            
                if(erased)
                {
                    // if we could remove the awaiter
                    // we will notfy it
                    awaiter->Notify();
                }
            }

            LinkedPtrQueue<pop_awaiter_type>  _popAwaiters;
            LinkedPtrQueue<push_awaiter_type> _pushAwaiters;

            LinkedPtrOrderedList<listener_awaiter_type> _listenerWaiters;

            ContainerT<Element> _valueCollection;
            const size_t        _maxQueueSize;
            bool                _closed{false};

            // this is an optional cleanup function
            // If we call close this function will be called
            // for the rest of the elements in the queue (_valueCollection)
            cleanupFunction_t _cleanupFunction{nullptr};

            mutable std::mutex  _mtx;
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class BufferedChannelPopAwaiter
        {
        public:
            BufferedChannelPopAwaiter(ChannelT& channel, EventT event, ValueT& v)
            : _channel{channel}
            , _value{v}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            BufferedChannelPopAwaiter(BufferedChannelPopAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                return _channel.IsReady(this);
            }

            [[nodiscard]] constexpr bool await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                // after channel.Add never touch the _channel member again
                // it could be a dangling ref
                if (_channel.Add(this))
                {
                    return true;
                }
                
                // resume immediately
                ResumeFromPause(parentCoro);
                return false;
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

            void Notify() const noexcept
            {
                // Notify scheduler to put coroutine back on CPU
                _event.Notify();
            }

            void Cancel() noexcept { _channel.Cancel(this); }

            template <typename T>
            void SetValue(T&& value, bool lastElement)
            {
                assert(_set == false);

                _value       = std::forward<T>(value);
                _lastElement = lastElement;
                _set         = true;
            }

            BufferedChannelPopAwaiter* next{nullptr};

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

            ChannelT& _channel;
            ValueT&   _value;
            EventT    _event;
        };

        template <typename ChannelT, typename EventT>
        class BufferedChannelListenerAwaiter
        {
        public:
            BufferedChannelListenerAwaiter(ChannelT& channel, EventT event, size_t count)
            : _channel{channel}
            , _event{std::move(event)}
            , _listenersCount{count}
            {
            }

            // disable move and copy
            BufferedChannelListenerAwaiter(BufferedChannelListenerAwaiter&&) = delete;

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

            void Notify() const noexcept
            {
                // Notify scheduler to put coroutine back on CPU
                _event.Notify();
            }

            void Cancel() noexcept { _channel.Cancel(this); }

            BufferedChannelListenerAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                context::UnpauseTask(parentCoro);
            }

            ChannelT&    _channel;
            EventT       _event;
            const size_t _listenersCount;
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class BufferedChannelPushAwaiter
        {
            using cleanupFunction_t = std::function<void(ValueT&)>;

        public:
            template <typename... Args>
            BufferedChannelPushAwaiter(ChannelT& channel, EventT event, cleanupFunction_t cleanupFunc, bool lastValue, Args&&... args)
            : _channel{channel}
            , _event{std::move(event)}
            , _value{std::forward<Args>(args)...}
            , _lastElement{lastValue}
            , _cleanupFunction{std::move(cleanupFunc)}
            {
            }

            // disable move and copy
            BufferedChannelPushAwaiter(BufferedChannelPushAwaiter&&) = delete;

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

            constexpr auto await_resume() noexcept
            {
                // after await_suspend channel can be a dangling reference
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

            void Notify() const noexcept
            {
                // Notify scheduler to put coroutine back on CPU
                _event.Notify();
            }

            void Cancel() noexcept { _channel.Cancel(this); }

            BufferedChannelPushAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(context::PauseTask(parentCoro)); }

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

            // Flag which is true if the value is in use
            bool _used{false};

            // this is an optinal cleanup function.
            // If the channel is closed and the value stuck in the awaiter
            // we will perform this operation on the value as cleanup
            cleanupFunction_t _cleanupFunction;
        };

        template <typename ValueT>
        using Queue = std::deque<ValueT>;

    } // namespace detail

    template <typename ValueT>
    using BufferedChannel = detail::
        BufferedChannel<ValueT, detail::BufferedChannelPopAwaiter, detail::BufferedChannelListenerAwaiter, detail::BufferedChannelPushAwaiter, detail::Queue>;

} // namespace tinycoro

#endif //!__TINY_CORO_BUFFERED_CHANNEL_HPP__