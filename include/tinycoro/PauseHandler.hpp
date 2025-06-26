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

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept PauseHandlerCb = std::regular_invocable<T>;

    } // namespace concepts

    namespace detail {

        struct PauseCallbackEvent
        {
        private:
            PauseHandlerCallbackT _notifyCallback;

        public:
            void Notify() const
            {
                if (_notifyCallback)
                {
                    _notifyCallback();
                }
            }

            template <typename T>
                requires requires (T&& t) {
                    { _notifyCallback = std::forward<T>(t) };
                }
            void Set(T&& cb)
            {
                _notifyCallback = std::forward<T>(cb);
            }
        };

    } // namespace detail

    class PauseHandler : public detail::IntrusiveObject<PauseHandler>
    {
        static constexpr uint8_t c_pauseMask{0x01};
        static constexpr uint8_t c_cancelMask{0x02};

    public:
        PauseHandler(auto pr, bool cancellable = tinycoro::default_initial_cancellable_policy::value)
        : _resumerCallback{pr}
        {
            // Set the initial cancellable flag: true or false.
            //
            // If set to true, the coroutine is cancellable before it starts.
            // The flag is reset after the coroutine starts.
            if(cancellable)
                SetCancellable(cancellable);
        }

        void Resume() noexcept
        {
            _state.store(0u, std::memory_order::relaxed);
        }

        void ResetCallback(concepts::PauseHandlerCb auto pr) { _resumerCallback = std::move(pr); }

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

        [[nodiscard]] bool IsPaused() const noexcept { return _state.load(std::memory_order_relaxed) & c_pauseMask; }

        [[nodiscard]] bool IsCancellable() const noexcept { return _state.load(std::memory_order_relaxed) & c_cancelMask; }

    private:
        // Placing _state before _resumerCallback saves 8 bytes of padding
        // on all three major compilers: MSVC, GCC, and Clang.
        std::atomic<uint8_t> _state{};

        PauseHandlerCallbackT _resumerCallback{};
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