#ifndef __TINY_CORO_CANCELLABLE_HPP__
#define __TINY_CORO_CANCELLABLE_HPP__

#include "PauseHandler.hpp"
#include "Finally.hpp"
#include "StaticStorage.hpp"
#include "Common.hpp"

namespace tinycoro {

    // Making a single suspension point
    // explicitly cancellable
    // Works only with Awaitables which has cancellation support. 
    // (e.g. tinycoro::Task has NO cancellation support)
    template <concepts::IsCancellableAwait AwaiterT>
    class Cancellable
    {
        using stopCallback_t = std::stop_callback<std::function<void()>>;
        using StorageT       = detail::StaticStorage<stopCallback_t, sizeof(stopCallback_t)>;

    public:
        // accepts only r-value refs
        Cancellable(AwaiterT&& awaiter)
        : _awaiter{std::addressof(awaiter)}
        {
            assert(_awaiter);
        }

        // disable move and copy
        Cancellable(Cancellable&&) = delete;

        [[nodiscard]] constexpr bool await_ready() noexcept
        {
            // delegate the call to the awaiter
            return _awaiter->await_ready();
        }

        constexpr auto await_suspend(auto parentCoro)
        {
            auto suspend = _awaiter->await_suspend(parentCoro);

            if(suspend)
            {
                assert(parentCoro.promise().stopSource.stop_possible());

                // make the parent coroutine cancellable
                context::MakeCancellable(parentCoro);

                if (parentCoro.promise().stopSource.stop_possible())
                {
                    // if we have a valid stop_source
                    // we also setup a stop_callback
                    _stopCallback.Construct<stopCallback_t>(parentCoro.promise().stopSource.get_token(), [aw = _awaiter] { aw->Cancel(); });
                }
            }

            return suspend;
        }

        constexpr auto await_resume() noexcept
        {
            // delegate the call to the awaiter
            return _awaiter->await_resume();
        }

    private:
        AwaiterT* _awaiter;
        StorageT  _stopCallback;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_CANCELLABLE_HPP__