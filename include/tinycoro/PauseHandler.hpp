#ifndef __TINY_CORO_PAUSE_HANDLER_HPP__
#define __TINY_CORO_PAUSE_HANDLER_HPP__

#include <functional>
#include <memory>
#include <concepts>
#include <coroutine>
#include <atomic>

#include "Common.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept PauseHandler = std::constructible_from<T, PauseHandlerCallbackT> && requires (T t) {
            { t.IsPaused() } -> std::same_as<bool>;
        };

        template <typename T>
        concept PauseHandlerCb = std::regular_invocable<T>;

    } // namespace concepts

    namespace detail {

        struct PauseCallbackEvent
        {
        private:
            std::function<void()> _notifyCallback;

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

    struct PauseHandler
    {
        PauseHandler(concepts::PauseHandlerCb auto pr)
        : _resumerCallback{pr}
        {
        }

        void Resume()
        {
            _cancellable.store(false, std::memory_order::relaxed);
            _pause.store(false, std::memory_order::relaxed);
            _pause.notify_all();
        }

        [[nodiscard]] auto Pause()
        {
            assert(_pause.load() == false);

            _pause.store(true, std::memory_order::relaxed);
            return _resumerCallback;
        }

        void Unpause()
        {
            assert(_pause.load() == true);

            _pause.store(false, std::memory_order::relaxed);
        }

        void SetCancellable(bool flag)
        {
            assert(_cancellable.load() == !flag);

            _cancellable.store(flag, std::memory_order::relaxed);
        }

        [[nodiscard]] bool IsPaused() const noexcept { return _pause.load(std::memory_order::relaxed); }

        [[nodiscard]] bool IsCancellable() const noexcept { return _cancellable.load(std::memory_order::relaxed); }

    private:
        PauseHandlerCallbackT _resumerCallback;

        std::atomic<bool>     _pause{false};
        std::atomic<bool>     _cancellable{false};
    };

    namespace context
    {
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

        /*template<typename T>
        void MakeCancellable(auto coroHdl, T&& returnValue)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            pauseHandlerPtr->SetCancellable(true);
            coroHdl.promise().return_value(std::forward<T>(returnValue));
        }*/

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
    }

} // namespace tinycoro

#endif //!__TINY_CORO_PAUSE_HANDLER_HPP__