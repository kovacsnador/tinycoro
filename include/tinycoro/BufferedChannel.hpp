#ifndef __TINY_CORO_BUFFERED_CHANNEL_HPP__
#define __TINY_CORO_BUFFERED_CHANNEL_HPP__

#include <mutex>
#include <queue>
#include <cassert>
#include <bitset>
#include <map>

#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace detail {

        enum class EOpStatus
        {
            SUCCESS,
            LAST,
            CLOSED
        };

        template <typename ValueT,
                  template <typename, typename, typename> class AwaiterT,
                  template <typename, typename> class ListenerAwaiterT,
                  template <typename> class ContainerT>
        class BufferedChannel
        {
            struct Element
            {
                ValueT value;
                bool   lastElement{false};
            };

        public:
            friend class AwaiterT<BufferedChannel, detail::PauseCallbackEvent, ValueT>;
            friend class ListenerAwaiterT<BufferedChannel, detail::PauseCallbackEvent>;

            using awaiter_type          = AwaiterT<BufferedChannel, detail::PauseCallbackEvent, ValueT>;
            using listener_awaiter_type = ListenerAwaiterT<BufferedChannel, detail::PauseCallbackEvent>;

            // default constructor
            BufferedChannel() {};

            // disable copy and move
            BufferedChannel(BufferedChannel&&) = delete;

            ~BufferedChannel() { Close(); }

            [[nodiscard]] auto PopWait(ValueT& val) { return awaiter_type{this, detail::PauseCallbackEvent{}, val}; }

            [[nodiscard]] auto WaitForListeners(size_t listenerCount)
            {
                return listener_awaiter_type{*this, detail::PauseCallbackEvent{}, listenerCount};
            }

            void Push(ValueT t) { _Emplace(std::move(t), false); }

            void PushAndClose(ValueT t) { _Emplace(std::move(t), true); }

            template <typename T>
            void Emplace(T&& t)
            {
                _Emplace(std::forward<T>(t), false);
            }

            template <typename T>
            void EmplaceAndClose(T&& t)
            {
                _Emplace(std::forward<T>(t), true);
            }

            [[nodiscard]] bool Empty() const noexcept
            {
                std::unique_lock lock{_mtx};
                return _valueCollection.empty();
            }

            void Close()
            {
                std::unique_lock lock{_mtx};
                _closed = true;

                auto top = _waiters.steal();
                lock.unlock();

                // notify all waiters
                _NotifyAll(top);
            }

            [[nodiscard]] bool IsOpen() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return !_closed;
            }

        private:
            template <typename T>
            void _Emplace(T&& t, bool close)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    throw BufferedChannelException{"BufferedChannel: channel is already closed."};
                }

                // Is there any awaiter
                if (auto* top = _waiters.pop())
                {
                    awaiter_type* others{nullptr};

                    if (close)
                    {
                        others = _waiters.steal();
                    }

                    _closed = close;

                    lock.unlock();

                    top->SetValue(std::forward<T>(t), close);
                    top->Notify();

                    if (_closed)
                    {
                        // notify all waiters
                        _NotifyAll(others);
                    }

                    return;
                }

                // No awaiters
                _valueCollection.emplace(Element{std::forward<T>(t), close});
            }

            [[nodiscard]] bool IsReady(awaiter_type* waiter) noexcept
            {
                std::unique_lock lock{_mtx};
                auto [ready, lastElement] = _SetValue(waiter);

                if (lastElement)
                {
                    auto waiters = _waiters.steal();
                    lock.unlock();

                    _NotifyAll(waiters);
                }

                return ready;
            }

            [[nodiscard]] bool Add(awaiter_type* waiter)
            {
                std::unique_lock lock{_mtx};

                auto [ready, lastElement] = _SetValue(waiter);
                auto suspend              = !ready;

                if (lastElement)
                {
                    auto waiters = _waiters.steal();
                    lock.unlock();

                    _NotifyAll(waiters);
                    return suspend;
                }

                if (suspend)
                {
                    _waiters.push(waiter);

                    auto iter = _listenerWaiters.find(_waiters.size());
                    if (iter != _listenerWaiters.end())
                    {
                        auto top = iter->second.steal();
                        lock.unlock();

                        // notify all if somebody waits for listerens
                        _NotifyAll(top);
                    }
                }

                // susend coroutine
                return suspend;
            }

            [[nodiscard]] bool IsReady(listener_awaiter_type* waiter) noexcept
            {
                std::scoped_lock lock{_mtx};
                return waiter->ListenerCount() <= _waiters.size();
            }

            [[nodiscard]] bool Add(listener_awaiter_type* waiter)
            {
                const auto wantedListerenCount = waiter->ListenerCount();

                std::unique_lock lock{_mtx};

                if (wantedListerenCount <= _waiters.size())
                {
                    // no suspend
                    return false;
                }

                // insert new listener waiter into the list
                _listenerWaiters[wantedListerenCount].push(waiter);

                // suspend coroutine
                return true;
            }

            // auto [ready, lastElement] = std::tuple<bool, bool>
            std::tuple<bool, bool> _SetValue(awaiter_type* waiter)
            {
                if (_closed)
                {
                    // channel is closed, no suspend
                    return {true, false};
                }

                if (_valueCollection.empty() == false)
                {
                    auto& [value, lastElement] = _valueCollection.front();

                    if (lastElement)
                    {
                        // close the channel, last element reached
                        _closed = true;
                    }

                    auto last = lastElement;

                    waiter->SetValue(std::move(value), lastElement);
                    _valueCollection.pop();

                    // we have a value from the channel, no suspend
                    return {true, last};
                }

                // channel is empty, suspend coroutine
                return {false, false};
            }

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

            LinkedPtrStack<awaiter_type>                            _waiters;
            std::map<size_t, LinkedPtrStack<listener_awaiter_type>> _listenerWaiters;

            ContainerT<Element> _valueCollection;
            bool                _closed{false};
            mutable std::mutex  _mtx;
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        class BufferedChannelAwaiter
        {
        public:
            BufferedChannelAwaiter(ChannelT* channel, EventT event, ValueT& v)
            : _channel{channel}
            , _value{v}
            , _event{std::move(event)}
            {
            }

            // disable move and copy
            BufferedChannelAwaiter(BufferedChannelAwaiter&&) = delete;

            [[nodiscard]] constexpr bool await_ready() noexcept
            {
                if (_channel)
                {
                    return _channel->IsReady(this);
                }
                return true;
            }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                if (_channel)
                {
                    PutOnPause(parentCoro);
                    if (_channel->Add(this) == false)
                    {
                        // resume immediately
                        ResumeFromPause(parentCoro);
                        return parentCoro;
                    }
                    return std::noop_coroutine();
                }
                return parentCoro;
            }

            [[nodiscard]] constexpr auto await_resume() const noexcept
            {
                if (_lastElement)
                {
                    return EOpStatus::LAST;
                }

                if (_set)
                {
                    return EOpStatus::SUCCESS;
                }

                return EOpStatus::CLOSED;
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

            BufferedChannelAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

            // Flag to check if this is the last element in the channel. (The channel is already in closed state)
            bool _lastElement{false};

            // Flag which is true if the value is set
            bool _set{false};

            ChannelT* _channel;
            ValueT&   _value;
            EventT    _event;
        };

        template <typename ChannelT, typename EventT>
        class ListenerAwaiter
        {
        public:
            ListenerAwaiter(ChannelT& channel, EventT event, size_t count)
            : _channel{channel}
            , _event{std::move(event)}
            , _listenersCount{count}
            {
            }

            // disable move and copy
            ListenerAwaiter(ListenerAwaiter&&) = delete;

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

            [[nodiscard]] auto ListenerCount() const noexcept { return _listenersCount; }

            void Notify()
            {
                // Notify scheduler to put coroutine back on CPU
                _event.Notify();
            }

            ListenerAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

            ChannelT&    _channel;
            EventT       _event;
            const size_t _listenersCount;
        };

        template <typename ValueT>
        using Queue = std::queue<ValueT>;

    } // namespace detail

    template <typename ValueT>
    using BufferedChannel = detail::BufferedChannel<ValueT, detail::BufferedChannelAwaiter, detail::ListenerAwaiter, detail::Queue>;

    using BufferedChannel_OpStatus = detail::EOpStatus;

} // namespace tinycoro

#endif //!__TINY_CORO_BUFFERED_CHANNEL_HPP__