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
        PauseHandler(concepts::PauseHandlerCb auto pr)
        : _resumerCallback{pr}
        {
        }

        void Resume()
        {
            _state.store(0u, std::memory_order::relaxed);
        }

        void ResetCallback(concepts::PauseHandlerCb auto pr) { _resumerCallback = std::move(pr); }

        [[nodiscard]] auto Pause()
        {
            assert(IsPaused() == false);
            _state.fetch_or(c_pauseMask, std::memory_order_release);
            return _resumerCallback;
        }

        void Unpause()
        {
            assert(IsPaused());
            _state.fetch_and(~c_pauseMask, std::memory_order_release);
        }

        void SetCancellable(bool flag)
        {
            assert(IsCancellable() != flag);

            if (flag)
                _state.fetch_or(c_cancelMask, std::memory_order_release);
            else
                _state.fetch_and(~c_cancelMask, std::memory_order_release);
        }

        [[nodiscard]] bool IsPaused() const noexcept { return _state & c_pauseMask; }

        [[nodiscard]] bool IsCancellable() const noexcept { return _state & c_cancelMask; }

    private:
        // Placing _state before _resumerCallback saves 8 bytes of padding
        // on all three major compilers: MSVC, GCC, and Clang.
        std::atomic<uint8_t> _state{};

        PauseHandlerCallbackT _resumerCallback;
    };

    namespace context {
        [[nodiscard]] auto PauseTask(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            return pauseHandlerPtr->Pause();
        }

        void MakeCancellable(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            pauseHandlerPtr->SetCancellable(true);
        }

        void UnpauseTask(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            pauseHandlerPtr->Unpause();
        }

        [[nodiscard]] auto* GetHandler(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            return pauseHandlerPtr;
        }
    } // namespace context

} // namespace tinycoro

#endif // TINY_CORO_PAUSE_HANDLER_HPP