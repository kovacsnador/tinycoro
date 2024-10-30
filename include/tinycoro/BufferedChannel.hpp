#ifndef __TINY_CORO_BUFFERED_CHANNEL_HPP__
#define __TINY_CORO_BUFFERED_CHANNEL_HPP__

#include <mutex>
#include <queue>

#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace detail {

        enum class EOpStatus
        {
            SUCCESS,
            CLOSED
        };

        template <typename ValueT, template <typename, typename, typename> class AwaiterT, template <typename> class ContainerT>
        struct BufferedChannel
        {
            friend struct AwaiterT<BufferedChannel, PauseCallbackEvent, ValueT>;

            using awaiter_type = AwaiterT<BufferedChannel, PauseCallbackEvent, ValueT>;

            ~BufferedChannel()
            {
                Close();
            }

            [[nodiscard]] auto PopWait(ValueT& val) { return awaiter_type{*this, PauseCallbackEvent{}, val}; }

            template <typename T>
            void Push(T&& t)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    throw BufferedChannelException{"BufferedChannel: channel is already closed."};
                }

                // Is there any awaiter
                if (auto* top = _waiters.pop())
                {
                    top->SetValue(std::forward<T>(t));
                    lock.unlock();

                    top->Notify();
                    return;
                }

                // No awaiters
                _container.emplace(std::forward<T>(t));
            }

            [[nodiscard]] bool Empty() const noexcept
            {
                std::unique_lock lock{_mtx};
                return _container.empty();
            }

            void Close()
            {
                std::unique_lock lock{_mtx};
                _closed = true;

                // NOtify all waiters
                while (auto* waiter = _waiters.pop())
                {
                    waiter->Notify();
                }
            }

            [[nodiscard]] bool IsOpen() const noexcept
            {
                std::scoped_lock lock{_mtx};
                return !_closed;
            }

        private:
            [[nodiscard]] EOpStatus Resume() const noexcept { return _closed ? EOpStatus::CLOSED : EOpStatus::SUCCESS; }

            [[nodiscard]] bool IsReady(awaiter_type* waiter) noexcept
            {
                std::scoped_lock lock{_mtx};

                if(_closed)
                {
                    return true;
                }

                if (_container.empty() == false)
                {
                    waiter->SetValue(std::move(_container.front()));
                    _container.pop();
                    return true;
                }

                return false;
            }

            [[nodiscard]] bool Add(awaiter_type* waiter)
            {
                std::scoped_lock lock{_mtx};

                if(_closed)
                {
                    // channel is closed nothing to do
                    return false;
                }

                if (_container.empty() == false)
                {
                    waiter->SetValue(std::move(_container.front()));
                    _container.pop();
                    return false;
                }

                _waiters.push(waiter);
                return true;
            }

            LinkedPtrStack<awaiter_type> _waiters;
            ContainerT<ValueT>           _container;
            bool                         _closed{false};
            mutable std::mutex           _mtx;
        };

        template <typename ChannelT, typename EventT, typename ValueT>
        struct BufferedChannelAwaiter
        {
            BufferedChannelAwaiter(ChannelT& channel, EventT event, ValueT& v)
            : _channel{channel}
            , _value{v}
            , _event{std::move(event)}
            {
            }

            [[nodiscard]] constexpr bool await_ready() noexcept { return _channel.IsReady(this); }

            constexpr std::coroutine_handle<> await_suspend(auto parentCoro)
            {
                PutOnPause(parentCoro);
                if (_channel.Add(this) == false)
                {
                    // resume immediately
                    ResumeFromPause(parentCoro);
                    return parentCoro;
                }
                return std::noop_coroutine();
            }

            [[nodiscard]] constexpr auto await_resume() const noexcept { return _channel.Resume(); }

            void Notify() const { _event.Notify(); }

            template <typename T>
            void SetValue(T&& value)
            {
                _value = std::forward<T>(value);
            }

            BufferedChannelAwaiter* next{nullptr};

        private:
            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

            ChannelT& _channel;
            ValueT&   _value;
            EventT    _event;
        };

    } // namespace detail

    template <typename ValueT>
    using BufferedChannel = detail::BufferedChannel<ValueT, detail::BufferedChannelAwaiter, std::queue>;

    using BufferedChannel_OpStatus = detail::EOpStatus;

} // namespace tinycoro

#endif //!__TINY_CORO_BUFFERED_CHANNEL_HPP__