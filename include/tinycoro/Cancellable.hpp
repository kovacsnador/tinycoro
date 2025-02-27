#ifndef __TINY_CORO_CANCELLABLE_HPP__
#define __TINY_CORO_CANCELLABLE_HPP__

#include "PauseHandler.hpp"
#include "Finally.hpp"
#include "StaticStorage.hpp"

namespace tinycoro {

    // Making a single suspension point
    // explicitly cancellable
    template <typename AwaiterT>
    class Cancellable
    {
        using stopCallback_t = std::stop_callback<std::function<void()>>;
        using StorageT       = detail::StaticStorage<stopCallback_t, sizeof(stopCallback_t)>;

    public:
        Cancellable(AwaiterT&& awaiter)
        : _awaiter{std::addressof(awaiter)}
        {
            assert(_awaiter);
        }

        Cancellable(AwaiterT& awaiter)
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
            auto makeCancellableOnExit = Finally([this, parentCoro] {
                // make the parent coroutine cancellable
                context::MakeCancellable(parentCoro);

                if (parentCoro.promise().stopSource.stop_possible())
                {
                    // if we have a valid stop_source
                    // we also setup a stop_callback
                    _stopCallback.Construct<stopCallback_t>(parentCoro.promise().stopSource.get_token(), [aw = _awaiter] { aw->Notify(); });
                }
            });

            // delegate the call to the awaiter
            return _awaiter->await_suspend(parentCoro);
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