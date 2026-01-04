// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_PAUSE_HANDLER_HPP
#define TINY_CORO_PAUSE_HANDLER_HPP

#include <functional>
#include <memory>
#include <concepts>
#include <coroutine>
#include <atomic>

#include "Common.hpp"
#include "IntrusiveObject.hpp"
#include "CallOnce.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept IsResumeCallbackType = std::regular_invocable<T, ENotifyPolicy>;

    } // namespace concepts

    class PauseHandler : public detail::IntrusiveObject<PauseHandler>
    {
        static constexpr uint8_t c_pauseMask{0x01};
        static constexpr uint8_t c_cancelMask{0x02};
        static constexpr uint8_t c_exceptionMask{0x04};
        static constexpr uint8_t c_observerMask{0x08};

        [[nodiscard]] constexpr auto _IsSet(auto mask, std::memory_order order = std::memory_order::relaxed) const noexcept 
        { 
            return _state.load(order) & mask;
        }

    public:
        PauseHandler(auto pr, bool cancellable = tinycoro::default_initial_cancellable_policy::value, EOwnPolicy ownPolicy = EOwnPolicy::OWNER)
        : _resumerCallback{pr}
        {
            // Set the initial cancellable flag: true or false.
            //
            // If set to true, the coroutine is cancellable before it starts.
            // The flag is reset after the coroutine starts.
            if(cancellable)
                SetCancellable(cancellable);

            if (ownPolicy == EOwnPolicy::OBSERVER)
                _MarkObserver();
        }

        void Resume() noexcept
        {
            // Reset all state flags before coroutine resumption,
            // except the exception and the observer flag.
            // 
            // Those flags are persistent—
            // once set, it cannot be cleared.
            _state.fetch_and(c_exceptionMask | c_observerMask, std::memory_order_relaxed);
        }

        void ResetCallback(concepts::IsResumeCallbackType auto pr) { _resumerCallback = std::move(pr); }

        [[nodiscard]] auto Pause() noexcept
        {
            assert(IsPaused() == false);
            _state.fetch_or(c_pauseMask, std::memory_order_relaxed);
            return _resumerCallback;
        }

        void Unpause() noexcept
        {
            assert(IsPaused());
            _state.fetch_and(~c_pauseMask, std::memory_order_relaxed);
        }

        void SetCancellable(bool flag) noexcept
        {
            assert(IsCancellable() != flag);

            if (flag)
                _state.fetch_or(c_cancelMask, std::memory_order_relaxed);
            else
                _state.fetch_and(~c_cancelMask, std::memory_order_relaxed);
        }

        void MarkException() noexcept
        {
            assert(HasException() == false);
            _state.fetch_or(c_exceptionMask, std::memory_order_relaxed);
        }

        [[nodiscard]] bool HasException() const noexcept { return _IsSet(c_exceptionMask); }

        [[nodiscard]] bool IsObserver() const noexcept { return _IsSet(c_observerMask); }

        [[nodiscard]] bool IsPaused() const noexcept { return _IsSet(c_pauseMask); }

        [[nodiscard]] bool IsCancellable() const noexcept { return _IsSet(c_cancelMask); }

    private:
        // Marks the promise as an observer promise
        //
        // That means that the schedulabe Promise is not owning
        // the std::corouitne_handler and therefore it does not
        // call the corresponding destroy function.
        void _MarkObserver() noexcept
        {
            assert(IsObserver() == false);
            _state.fetch_or(c_observerMask, std::memory_order_relaxed);
        }

        // Placing _state before _resumerCallback saves 8 bytes of padding
        // on all three major compilers: MSVC, GCC, and Clang.
        std::atomic<uint8_t> _state{};

        // The resume callback, which is intented to
        // resume the coroutine from a suspension state.
        ResumeCallback_t _resumerCallback{};
    };

    namespace context {
        [[nodiscard]] auto PauseTask(auto coroHdl) noexcept
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            return pauseHandlerPtr->Pause();
        }

        void MakeCancellable(auto coroHdl) noexcept
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            pauseHandlerPtr->SetCancellable(true);
        }

        void UnpauseTask(auto coroHdl) noexcept
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            pauseHandlerPtr->Unpause();
        }

        [[nodiscard]] auto* GetHandler(auto coroHdl) noexcept
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            return pauseHandlerPtr;
        }
    } // namespace context

} // namespace tinycoro

#endif // TINY_CORO_PAUSE_HANDLER_HPP