// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_SHARED_STATE_HPP
#define TINY_CORO_SHARED_STATE_HPP

#include <cassert>
#include <variant>

#include "Common.hpp"

namespace tinycoro {
    namespace detail {

        struct SharedState
        {
            // the first 2 bits are reserved for pausestate
            // see EPauseState in Common.hpp
            static constexpr uint8_t c_pauseMask{1 << 2};
            static constexpr uint8_t c_cancelMask{1 << 3};

            // Those flags are presistent, and will be not cleared
            // with every coroutine resume in ClearFlags().
            static constexpr uint8_t c_exceptionMask{1 << 4};
            static constexpr uint8_t c_stopTokenUser{1 << 5};

            [[nodiscard]] auto _IsSet(auto flag) const noexcept { return _state.load(std::memory_order::relaxed) & flag; }

        public:
            constexpr SharedState(bool cancellable)
            {
                // Set the initial cancellable flag: true or false.
                //
                // If set to true, the coroutine is cancellable before it starts.
                // The flag is reset after the coroutine starts.
                if (cancellable)
                    MakeCancellable();
            }

            void ClearFlags() noexcept
            {
                // Reset all state flags before coroutine resumption,
                // except the exception flag and the stopTokenUser. The exception flag is persistent—
                // once set, it cannot be cleared.
                _state.fetch_and(c_exceptionMask | c_stopTokenUser, std::memory_order_relaxed);
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

            void MakeCancellable() noexcept
            {
                assert(IsCancellable() == false);
                _state.fetch_or(c_cancelMask, std::memory_order_relaxed);
            }

            void MarkException() noexcept
            {
                assert(HasException() == false);
                _state.fetch_or(c_exceptionMask, std::memory_order_relaxed);
            }

            void MarkStopTokenUser() noexcept
            {
                assert(IsStopTokenUser() == false);
                _state.fetch_or(c_stopTokenUser, std::memory_order_relaxed);
            }

            [[nodiscard]] bool HasException() const noexcept { return _IsSet(c_exceptionMask); }

            [[nodiscard]] bool IsPaused() const noexcept { return _IsSet(c_pauseMask); }

            [[nodiscard]] bool IsCancellable() const noexcept { return _IsSet(c_cancelMask); }

            [[nodiscard]] bool IsStopTokenUser() const noexcept { return _IsSet(c_stopTokenUser); }

            [[nodiscard]] auto Load(std::memory_order order) const noexcept { return _state.load(order); }

            template <typename T>
            [[nodiscard]] bool CompareExchange(T& expected, T desired, std::memory_order orderSucceed, std::memory_order orderFailed) noexcept
            {
                return _state.compare_exchange_strong(expected, desired, orderSucceed, orderFailed);
            }

            auto ClearPauseStateBits(std::memory_order order = std::memory_order::relaxed) noexcept
            {
                // clear the first bits which are responsible for the pause state in scheduler.
                return _state.fetch_and(~detail::UTypeCast(detail::EPauseState::IDLE), order);
            };

        private:
            // TODO: prove that.
            // Placing _state before _resumerCallback saves 8 bytes of padding
            // on all three major compilers: MSVC, GCC, and Clang.
            std::atomic<uint8_t> _state{};

            // The resume callback, which is intented to
            // resume the coroutine from a suspension state.
            ResumeCallback_t _resumerCallback{};
        };

        using SharedState_t = std::variant<detail::SharedState*, detail::SharedState>;

    } // namespace detail

    namespace context {

        // Pauses the task associated with the given coroutine handle.
        //
        // The pause request is forwarded to the task's shared state.
        //
        // Returns a ResumeCallback_t which is responsible for the task resumption.
        [[nodiscard]] ResumeCallback_t PauseTask(auto hdl) noexcept
        {
            return hdl.promise().SharedState()->Pause();
        }

        // Marks the task associated with the given coroutine handle as cancellable.
        //
        // After calling this function, the task may react to cancellation
        // requests according to its shared state implementation.
        void MakeCancellable(auto hdl) noexcept
        {
            return hdl.promise().SharedState()->MakeCancellable();
        }

        // Restores the task state and settign the flag to unpaused.
        //
        // The unpause request is forwarded to the task's shared state.
        void UnpauseTask(auto hdl) noexcept
        {
            return hdl.promise().SharedState()->Unpause();
        }
    } // namespace context

} // namespace tinycoro

#endif // TINY_CORO_SHARED_STATE_HPP