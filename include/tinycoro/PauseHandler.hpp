#ifndef __TINY_CORO_PAUSE_HANDLER_HPP__
#define __TINY_CORO_PAUSE_HANDLER_HPP__

#include <functional>
#include <memory>
#include <concepts>
#include <coroutine>
#include <atomic>

namespace tinycoro {

    struct PauseHandler;

    namespace concepts {

        template <typename T>
        concept PauseHandler = requires (T t) {
            { t.pause.load() } -> std::same_as<bool>;
            requires std::invocable<decltype(t.pauseResume), std::shared_ptr<tinycoro::PauseHandler>>;
        };

        template <typename T>
        concept PauseHandlerCb = std::invocable<T, std::shared_ptr<tinycoro::PauseHandler>>;

    } // namespace concepts

    using PauseHandlerCallbackT = std::function<void(std::shared_ptr<PauseHandler>)>;

    struct PauseHandler
    {
        PauseHandler(concepts::PauseHandlerCb auto pr)
        : pauseResume{pr}
        {
        }

        [[nodiscard]] static auto PauseTask(auto& coroHdl)
        {
            auto pauseHandler = coroHdl.promise().pauseHandler;
            assert(pauseHandler);
            pauseHandler->pause.store(true);
            return [pauseHandler] { pauseHandler->pauseResume(pauseHandler); };
        }

        PauseHandlerCallbackT pauseResume;
        std::atomic<bool>     pause{false};
    };

} // namespace tinycoro

#endif //!__TINY_CORO_PAUSE_HANDLER_HPP__