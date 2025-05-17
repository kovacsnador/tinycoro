#ifndef TINY_CORO_SCHEDULABLE_TASK_HPP
#define TINY_CORO_SCHEDULABLE_TASK_HPP

#include <stop_token>
#include <cassert>

#include "TaskResumer.hpp"
#include "Promise.hpp"
#include "Common.hpp"

namespace tinycoro { namespace detail {

    // This is the Schedulable task which is used
    // by the scheudler.
    //
    //
    // Provides a simple wrapper around the
    // std:coroutine_handle<>, and implements some
    // smart pointer identic API, but only for
    // compatibility reasons.
    template <typename PromiseT, typename CoroResumerT = TaskResumer, typename StopSourceT = std::stop_source>
    class SchedulableTaskT
    {
    public:
        using Promise_t    = PromiseT;
        using element_type = PromiseT;

        SchedulableTaskT() = default;

        SchedulableTaskT(PromiseT* promise)
        {
            if (promise)
            {
                _handle = std::coroutine_handle<PromiseT>::from_promise(*promise);
            }
        }

        SchedulableTaskT(SchedulableTaskT&& other) noexcept
        : _handle{std::exchange(other._handle, nullptr)}
        {
        }

        SchedulableTaskT& operator=(SchedulableTaskT&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                Destroy();
                _handle = std::exchange(other._handle, nullptr);
            }
            return *this;
        }

        ~SchedulableTaskT() { Destroy(); }

        [[nodiscard]] inline auto Resume() noexcept
        {
            try
            {
                // resume the corouitne
                _coroResumer.Resume(_handle.promise());
            }
            catch (...)
            {
                // if there was an exception
                // save it in the promise.
                _handle.promise().exception = std::current_exception();
            }

            // return the corouitne state.
            return _coroResumer.ResumeState(_handle);
        }

        auto SetPauseHandler(concepts::PauseHandlerCb auto pauseResume) noexcept
        {

            auto& pauseHandler = _handle.promise().pauseHandler;
            if (pauseHandler)
            {
                // pause handler is already initialized
                pauseHandler->ResetCallback(std::move(pauseResume));
            }
            else
            {
                // first the pause handler need to be initialized
                _handle.promise().MakePauseHandler(std::move(pauseResume));
            }

            return pauseHandler.get();
        }

        auto release() noexcept -> PromiseT*
        {
            assert(_handle);

            auto& promise = _handle.promise();
            _handle       = nullptr;
            return std::addressof(promise);
        }

        void reset(PromiseT* newPromise) noexcept
        {
            Destroy();

            if (newPromise)
            {
                _handle = std::coroutine_handle<PromiseT>::from_promise(*newPromise);
            }
        }

        constexpr bool operator==(std::nullptr_t) const noexcept { return _handle == nullptr; }

        // To mimic the smart pointer api
        constexpr auto* operator->() noexcept { return this; }

        auto get() noexcept
        {
            assert(_handle);
            return std::addressof(_handle.promise());
        }

        auto& PauseState() noexcept { return _handle.promise().pauseState; }

    private:
        void Destroy() noexcept
        {
            if (_handle)
            {
                _handle.destroy();
                _handle = nullptr;
            }
        }

        // contains special logic regarging
        // coroutine resumption and state
        [[no_unique_address]] CoroResumerT _coroResumer{};

        std::coroutine_handle<PromiseT> _handle{nullptr};
    };

    // this is the common task type
    // which hides the actual std::corouitne_handle<>
    // promise type.
    using SchedulableTask = SchedulableTaskT<tinycoro::detail::CommonPromise>;

    // This is the OnFinish callback
    // which is triggered in the promise base
    // in order to set the value in the 
    // corresponding promise object.
    template <typename PromiseT, typename FutureStateT>
    void OnTaskFinish(void* self, void* futureStatePtr)
    {
        auto promise = static_cast<PromiseT*>(self);
        auto future  = static_cast<FutureStateT*>(futureStatePtr);

        if (promise->exception)
        {
            // if we had an exception we just set it
            future->set_exception(promise->exception);
        }
        else
        {
            auto handle = std::coroutine_handle<PromiseT>::from_promise(*promise);
            if (handle.done())
            {
                // are we on a last suspend point?
                // That means we had no cancellation before
                if constexpr (requires { { promise->return_void() } -> std::same_as<void>; })
                {
                    future->set_value(VoidType{});
                }
                else
                {
                    future->set_value(promise->value());
                }
            }
            else
            {
                // the task got cancelled
                // we give back an empty optional
                future->set_value(std::nullopt);
            }
        }
    }

    // The schedulable task factory function.
    //
    // Intented to save and connect the future state
    // (e.g std::promise<>) object with the corouitne itself.
    template <concepts::IsCorouitneTask CoroT, concepts::FutureState FutureStateT>
        requires (!std::is_reference_v<CoroT>) && std::derived_from<typename CoroT::promise_type, detail::SchedulableTask::Promise_t>
    auto MakeSchedulableTask(CoroT&& coro, FutureStateT&& futureState)
    {
        // create std::corouitne_handle with
        // the common promise type
        auto  originalHandle = coro.Release();
        auto& promise        = originalHandle.promise();

        using PromiseT = CoroT::promise_type;

        // save the future state inside
        // the corouitne promise
        promise.SavePromise(std::move(futureState), detail::OnTaskFinish<PromiseT, FutureStateT>);

        // create the common task
        return detail::SchedulableTask{std::addressof(promise)};
    }

}} // namespace tinycoro::detail

#endif // TINY_CORO_SCHEDULABLE_TASK_HPP