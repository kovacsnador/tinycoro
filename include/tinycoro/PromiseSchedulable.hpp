#ifndef TINY_CORO_PROMISE_SCHEDULABLE_HPP
#define TINY_CORO_PROMISE_SCHEDULABLE_HPP

#include <concepts>

#include "PromiseBase.hpp"
#include "Common.hpp"
#include "AnyObject.hpp"

namespace tinycoro
{
    namespace detail
    {

// If you want to use your own promise/future type,
// and the default buffer size is too small,
// adjust this value accordingly.
#ifndef CUSTOM_PROMISE_BUFFER_SIZE
    static constexpr std::size_t PROMISE_BASE_BUFFER_SIZE = sizeof(std::promise<int64_t>);
#else
    static constexpr std::size_t PROMISE_BASE_BUFFER_SIZE = CUSTOM_PROMISE_BUFFER_SIZE;
#endif

    template <std::unsigned_integral auto BUFFER_SIZE, concepts::IsAwaiter FinalAwaiterT, concepts::PauseHandler PauseHandlerT, typename StopSourceT>
    struct SchedulablePromise : PromiseBase<FinalAwaiterT, PauseHandlerT, StopSourceT>, detail::DoubleLinkable<SchedulablePromise<BUFFER_SIZE, FinalAwaiterT, PauseHandlerT, StopSourceT>>
    {
        static_assert(BUFFER_SIZE >= PROMISE_BASE_BUFFER_SIZE, "SchedulablePromise: Buffer size is too small to hold the promise object.");

        using OnFinishCallback_t = void (*)(void*, void*);

        using PromiseBase_t = SchedulablePromise;

        SchedulablePromise() = default;

        // Disallow copy and move
        SchedulablePromise(SchedulablePromise&&) = delete;

        // Pause state needed by the scheduler.
        std::atomic<EPauseState> pauseState{EPauseState::IDLE};

        // Stores the exception pointer
        // if there was an unhandled_exception.
        //
        // It is set in the SchedulableTaskT.
        std::exception_ptr exception{nullptr};

        // Saves the promise inside the coroutine promise object
        // "PromiseT" must not be an L value reference.  
        template <typename PromiseT, typename OnFinishCallbackT>
            requires (!std::is_lvalue_reference_v<PromiseT>)
        void SavePromise(PromiseT&& promise, OnFinishCallbackT finishCb)
        {
            _onFinish = finishCb;
            _futureStateBuffer.template Emplace<PromiseT>(std::forward<PromiseT>(promise));
        }

        // Saves the underlying coroutine function
        // in the promise object itself.
        void SaveAnyFunction(detail::AnyObject&& anyFunc)
        {
            assert(_anyFunction == false);
            _anyFunction = std::move(anyFunc);
        }

        void Finish() noexcept
        {
            // This logic was previously in the destructor of SchedulablePromise,
            // but that caused a problem: the typed Promise, which holds
            // the return value, gets destroyed before the base Promise.
            //
            // so exported in a separete funcion, and need to be invoked
            // before the corouitne destroy call.
            if (_onFinish)
            {
                assert(_futureStateBuffer);

                // setting the promise object
                // if there is one connected
                _onFinish(this, _futureStateBuffer.RawData());
            }
        }

    private:
        // this is the on finish callback
        // It is only invoked, if the corouitne is done.
        //
        // The first void* parameter is the _promise object.
        // In that point only the _onFinish callback
        // knows the real type of the promise.
        //
        // This _onFinish needs to be invoked in the
        // derived promise destructor, the reason is, it uses
        // the return value of the task from the derived promise.
        OnFinishCallback_t _onFinish{nullptr};

        // buffer to store the promise object
        // NOT the coroutine promise, but the
        // promise like std::promise<>
        // or tinycoro::detail::UnsafePromise<>
        detail::SimpleStorage<BUFFER_SIZE> _futureStateBuffer;

        // Holds the underlying corouinte function
        // in case we use the BoundTask() idiom
        detail::AnyObject _anyFunction{};
    };
        
    } // namespace detail
    
} // namespace tinycoro


#endif //TINY_CORO_PROMISE_SCHEDULABLE_HPP