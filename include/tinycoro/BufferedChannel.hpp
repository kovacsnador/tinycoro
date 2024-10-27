#ifndef __TINY_CORO_BUFFERED_CHANNEL_HPP__
#define __TINY_CORO_BUFFERED_CHANNEL_HPP__

#include <mutex>
#include <queue>

#include "PauseHandler.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace detail {

        template <typename ValueT, template <typename, typename> class AwaiterT, template <typename> class ContainerT>
        struct BufferedChannel
        {
            friend struct AwaiterT<BufferedChannel, PauseCallbackEvent>;

            using awaiter_type = AwaiterT<BufferedChannel, PauseCallbackEvent>;

            [[nodiscard]] auto operator co_await() { return awaiter_type{*this, PauseCallbackEvent{}}; }

            template <typename T>
            void Push(T&& t)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    throw BufferedChannelException{"BufferedChannel: channel is already closed."};
                }

                _container.emplace(std::forward<T>(t));

                if (auto w = _waiter)
                {
                    _waiter = nullptr;
                    lock.unlock();

                    w->Notify();
                }
            }

            template <typename T>
            void Close(T&& t)
            {
                std::unique_lock lock{_mtx};

                if (_closed)
                {
                    throw BufferedChannelException{"BufferedChannel: channel is already closed."};
                }

                _closed = true;

                _container.emplace(std::forward<T>(t));

                if (auto w = _waiter)
                {
                    _waiter = nullptr;
                    lock.unlock();

                    w->Notify();
                }
            }

            [[nodiscard]] bool IsOpen() noexcept
            {
                std::scoped_lock lock{_mtx};
                return !_closed || !_container.empty();
            }

        private:
            [[nodiscard]] auto Pop()
            {
                std::unique_lock lock{_mtx};

                ValueT value{std::move(_container.front())};
                _container.pop();

                lock.unlock();

                return value;
            }

            [[nodiscard]] bool IsReady(const awaiter_type* waiter)
            {
                std::scoped_lock lock{_mtx};

                if (_waiter && _waiter != waiter)
                {
                    throw BufferedChannelException{"BufferedChannel: Only 1 consumer is allowed at the time."};
                }

                _waiter = waiter;

                return !_container.empty();
            }

            [[nodiscard]] bool Add(const awaiter_type* waiter)
            {
                std::scoped_lock lock{_mtx};

                if (_waiter && _waiter != waiter)
                {
                    throw BufferedChannelException{"BufferedChannel: Only 1 consumer is allowed at the time."};
                }

                return _container.empty();

                /*if (_container.empty())
                {
                    _waiter = waiter;
                    return true;
                }

                return false;*/
            }

            const awaiter_type* _waiter{nullptr};
            ContainerT<ValueT>  _container;
            bool                _closed{false};
            std::mutex          _mtx;
        };

        template <typename ChannelT, typename EventT>
        struct BufferedChannelAwaiter
        {
            BufferedChannelAwaiter(ChannelT& channel, EventT event)
            : _channel{channel}
            , _event{std::move(event)}
            {
            }

            [[nodiscard]] constexpr bool await_ready() const { return _channel.IsReady(this); }

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

            // Moving out the value.
            [[nodiscard]] constexpr auto await_resume() const noexcept { return _channel.Pop(); }

            void Notify() const { _event.Notify(); }

        private:
            void PutOnPause(auto parentCoro) { _event.Set(PauseHandler::PauseTask(parentCoro)); }

            void ResumeFromPause(auto parentCoro)
            {
                _event.Set(nullptr);
                PauseHandler::UnpauseTask(parentCoro);
            }

            ChannelT& _channel;
            EventT    _event;
        };

    } // namespace detail

    template <typename ValueT>
    using BufferedChannel = detail::BufferedChannel<ValueT, detail::BufferedChannelAwaiter, std::queue>;

} // namespace tinycoro

#endif //!__TINY_CORO_BUFFERED_CHANNEL_HPP__