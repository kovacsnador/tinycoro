#ifndef TINY_CORO_CANCELLABLE_HPP
#define TINY_CORO_CANCELLABLE_HPP

#include "PauseHandler.hpp"
#include "Finally.hpp"
#include "StaticStorage.hpp"
#include "Common.hpp"

namespace tinycoro {

    // Making a single suspension point
    // explicitly cancellable
    // Works only with Awaitables which has cancellation support.
    //
    // e.g. tinycoro::Task has NO cancellation support,
    // because a task is not a cancellation point.
    template <concepts::IsCancellableAwait AwaiterT>
    class Cancellable
    {
        using stopCallback_t = std::stop_callback<std::function<void()>>;
        using StorageT       = detail::StaticStorage<stopCallback_t, sizeof(stopCallback_t), stopCallback_t>;

    public:
        // accepts only r-value refs
        explicit Cancellable(AwaiterT&& awaiter)
        : _awaiter{awaiter}
        {
        }

        // disable move and copy
        Cancellable(Cancellable&&) = delete;

        [[nodiscard]] constexpr bool await_ready() noexcept
        {
            // delegate the call to the awaiter
            return _awaiter.await_ready();
        }

        constexpr auto await_suspend(auto parentCoro)
        {
            auto suspend = _awaiter.await_suspend(parentCoro);

            if (suspend)
            {
                auto& stopSource = parentCoro.promise().stopSource;

                if (stopSource.stop_possible())
                {
                    // if we have a valid stop_source
                    // we also setup a stop_callback
                    _stopCallback.Construct<stopCallback_t>(stopSource.get_token(), [this, parentCoro] {
                        if (_awaiter.Cancel())
                        {
                            // if we could cancel the awaiter
                            // this is the first point that we actually
                            // are able to mark the awaiter suspend as cancellable
                            context::MakeCancellable(parentCoro);

                            // after we own the awaiter and
                            // set the cancellable flag,
                            // we still need to notify the awaiter
                            // to trigger further actions.
                            _awaiter.Notify();
                        }
                    });
                }
            }

            return suspend;
        }

        constexpr auto await_resume() noexcept
        {
            // destroy the stop callback
            // we resume the coroutine anyway
            _stopCallback.reset();

            // delegate the call to the awaiter
            return _awaiter.await_resume();
        }

    private:
        AwaiterT& _awaiter;
        StorageT  _stopCallback;
    };

} // namespace tinycoro

#endif // TINY_CORO_CANCELLABLE_HPP