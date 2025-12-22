// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SCHEDULABLE_TASK_HPP
#define TINY_CORO_SCHEDULABLE_TASK_HPP

#include <stop_token>
#include <cassert>
#include <utility>

#include "TaskResumer.hpp"
#include "Promise.hpp"
#include "Common.hpp"
#include "DetachedTask.hpp"
#include "DetachedFuture.hpp"

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
                _hdl = std::coroutine_handle<PromiseT>::from_promise(*promise);
            }
        }

        SchedulableTaskT(SchedulableTaskT&& other) noexcept
        : _hdl{std::exchange(other._hdl, nullptr)}
        {
        }

        SchedulableTaskT& operator=(SchedulableTaskT&& other) noexcept
        {
            SchedulableTaskT{std::move(other)}.swap(*this);
            return *this;
        }

        ~SchedulableTaskT() { Destroy(); }

        [[nodiscard]] inline auto Resume() noexcept
        {
            try
            {
                // resume the corouitne
                _coroResumer.Resume(_hdl.promise());
            }
            catch (...)
            {
                // Calling directly the Finish() function,
                // if we have an exception.
                _hdl.promise().Finish(std::current_exception());
            }

            // return the corouitne state.
            return _coroResumer.ResumeState(_hdl);
        }

        void SetPauseHandler(concepts::IsResumeCallbackType auto pauseResume) noexcept
        {
            // At this point the pauseHandler should have been initialized
            // through the MakeSchedulableTask() function.
            auto& pauseHandler = _hdl.promise().pauseHandler;

            assert(pauseHandler);

            // pause handler should be already initialized
            pauseHandler->ResetCallback(std::move(pauseResume));
        }

        [[nodiscard]] auto release() noexcept -> PromiseT*
        {
            assert(_hdl);

            auto& promise = _hdl.promise();
            _hdl          = nullptr;
            return std::addressof(promise);
        }

        void reset(PromiseT* newPromise) noexcept
        {
            Destroy();

            if (newPromise)
            {
                _hdl = std::coroutine_handle<PromiseT>::from_promise(*newPromise);
            }
        }

        [[nodiscard]] constexpr bool operator==(std::nullptr_t) const noexcept { return _hdl == nullptr; }

        // To mimic the smart pointer api
        [[nodiscard]] constexpr auto* operator->() noexcept { return this; }

        [[nodiscard]] auto get() noexcept
        {
            assert(_hdl);
            return std::addressof(_hdl.promise());
        }

        [[nodiscard]] auto& PauseState() noexcept { return _hdl.promise().pauseState; }

        void swap(SchedulableTaskT& other) noexcept { std::swap(other._hdl, _hdl); }

    private:
        void Destroy() noexcept
        {
            if (_hdl)
            {
                _hdl.destroy();
                _hdl = nullptr;
            }
        }

        // contains special logic regarging
        // coroutine resumption and state
        [[no_unique_address]] CoroResumerT _coroResumer{};

        std::coroutine_handle<PromiseT> _hdl{nullptr};
    };

    // this is the common task type
    // which hides the actual std::corouitne_handle<>
    // promise type.
    using SchedulableTask = SchedulableTaskT<tinycoro::detail::CommonSchedulablePromiseT>;

    // This is the OnFinish callback
    // which is triggered in the promise base
    // in order to set the value in the
    // corresponding promise object.
    template <typename PromiseT, typename FutureStateT>
    void OnTaskFinish(void* self, void* futureStatePtr, std::exception_ptr exception)
    {
        auto promise = static_cast<PromiseT*>(self);
        auto future  = static_cast<FutureStateT*>(futureStatePtr);

        if (exception)
        {
            // if we had an exception we just set it
            future->set_exception(std::move(exception));
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

    struct OnTaskFinishCallbackWrapper
    {
        template <typename PromiseT, typename FutureStateT>
        [[nodiscard]] constexpr static auto Get() noexcept
        {
            // Returns the function pointer
            // which we need to invoke on task
            // finish.
            return detail::OnTaskFinish<PromiseT, FutureStateT>;
        }
    };

    template <typename ReturnT, template <typename> class FutureStateT>
    struct FutureTypeGetter
    {
        // futureReturn_t is packed in a std::optinal
        using futureReturn_t = typename detail::FutureReturnT<ReturnT>::value_type;

        // this is the future state type
        //
        // e.g. std::promise<std::optinal<int32_t>>
        using futureState_t = FutureStateT<futureReturn_t>;

        // and this is the corresponding future type
        //
        // e.g. std::future<std::optinal<int32_t>>
        using future_t = decltype(std::declval<futureState_t>().get_future());
    };

    // The schedulable task factory function.
    //
    // Intented to save and connect the future state
    // (e.g std::promise<>) object with the corouitne itself.
    template <typename OnFinishCallbackT, concepts::IsSchedulable CoroT, concepts::FutureState FutureStateT>
        requires (!std::is_reference_v<CoroT>) && std::derived_from<typename CoroT::promise_type, detail::SchedulableTask::Promise_t>
    auto MakeSchedulableTask(CoroT&& coro, FutureStateT&& futureState) noexcept
    {
        if constexpr (tinycoro::detail::IsDetached<CoroT>::value)
        {
            // we want a detached task
            //
            // set the future to ready (no waiting on user side)
            futureState.set_value(std::nullopt);

            // create the schedulable task
            return MakeSchedulableTaskImpl<OnFinishCallbackT>(std::move(coro), detail::DetachedPromise{});
        }
        else
        {
            // create the schedulable task
            return MakeSchedulableTaskImpl<OnFinishCallbackT>(std::move(coro), std::move(futureState));
        }
    }

    template <typename OnFinishCallbackT, concepts::IsSchedulable CoroT, concepts::FutureState FutureStateT>
        requires (!std::is_reference_v<CoroT>) && std::derived_from<typename CoroT::promise_type, detail::SchedulableTask::Promise_t>
    auto MakeSchedulableTaskImpl(CoroT&& coro, FutureStateT&& futureState) noexcept
    {
        // create std::corouitne_handle with
        // the common promise type
        auto  originalHandle = coro.Release();
        auto& promise        = originalHandle.promise();

        using promise_t     = std::remove_cvref_t<CoroT>::promise_type;
        using futureState_t = std::remove_cvref_t<FutureStateT>;

        // save the future state inside
        // the corouitne promise
        promise.SavePromise(std::move(futureState), OnFinishCallbackT::template Get<promise_t, futureState_t>());

        // Initialize the pause handler with the appropriate initial cancellation policy.
        //
        // This is the right place to set it because this coroutine will act as a root
        // in the coroutine chain.
        promise.MakePauseHandler(nullptr, CoroT::initial_cancellable_policy_t::value);

        // create the universal schedulable task
        return detail::SchedulableTask{std::addressof(promise)};
    }

}} // namespace tinycoro::detail

#endif // TINY_CORO_SCHEDULABLE_TASK_HPP