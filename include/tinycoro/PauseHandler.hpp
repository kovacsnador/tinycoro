#ifndef __TINY_CORO_PAUSE_HANDLER_HPP__
#define __TINY_CORO_PAUSE_HANDLER_HPP__

#include <functional>
#include <memory>
#include <concepts>
#include <coroutine>
#include <atomic>

namespace tinycoro {

    struct PauseHandler;

    using PauseHandlerCallbackT = std::function<void()>;

    namespace concepts {

        template <typename T>
        concept PauseHandler = std::constructible_from<T, PauseHandlerCallbackT> && requires (T t) {
            { t.IsPaused() } -> std::same_as<bool>;
        };

        template <typename T>
        concept PauseHandlerCb = std::regular_invocable<T>;

    } // namespace concepts

    struct PauseCallbackEvent
    {
        void Notify() const
        {
            if (_notifyCallback)
            {
                _notifyCallback();
            }
        }

        void Set(std::invocable auto cb)
        {
            assert(_notifyCallback == nullptr);

            _notifyCallback = cb;
        }

        std::function<void()> _notifyCallback;
    };

    struct PauseHandler
    {
        PauseHandler(concepts::PauseHandlerCb auto pr)
        : _pauseResume{pr}
        {
        }

        void Resume() { _pause.store(false, std::memory_order::relaxed); }

        [[nodiscard]] static auto PauseTask(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            assert(pauseHandlerPtr->_pause.load() == false);

            pauseHandlerPtr->_pause.store(true);

            return pauseHandlerPtr->_pauseResume;
        }

        [[nodiscard]] static auto* GetHandler(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);
            
            return pauseHandlerPtr;
        } 

        [[nodiscard]] bool IsPaused() const noexcept { return _pause.load(); }

    private:
        PauseHandlerCallbackT _pauseResume;
        std::atomic<bool>     _pause{false};
    };

} // namespace tinycoro

#endif //!__TINY_CORO_PAUSE_HANDLER_HPP__