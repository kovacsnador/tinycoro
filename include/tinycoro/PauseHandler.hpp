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
        : _pauseResume{pr}
        {
        }

        void Resume()
        {
            _pause.store(false, std::memory_order::relaxed);
            _pause.notify_all();
        }

        [[nodiscard]] static auto PauseTask(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            assert(pauseHandlerPtr->_pause.load() == false);

            pauseHandlerPtr->_pause.store(true);

            return pauseHandlerPtr->_pauseResume;
        }

        static void UnpauseTask(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            pauseHandlerPtr->_pause.store(false);
        }

        [[nodiscard]] static auto* GetHandler(auto coroHdl)
        {
            auto pauseHandlerPtr = coroHdl.promise().pauseHandler.get();
            assert(pauseHandlerPtr);

            return pauseHandlerPtr;
        }

        [[nodiscard]] bool IsPaused() const noexcept { return _pause.load(); }

        void AtomicWait(bool flag) const noexcept { _pause.wait(flag); }

    private:
        PauseHandlerCallbackT _pauseResume;
        std::atomic<bool>     _pause{false};
    };

} // namespace tinycoro

#endif //!__TINY_CORO_PAUSE_HANDLER_HPP__