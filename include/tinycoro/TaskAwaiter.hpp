#ifndef TINY_CORO_TASK_AWAITER_HPP
#define TINY_CORO_TASK_AWAITER_HPP

namespace tinycoro {
    template <typename CoroTaskT>
    struct AwaiterBase
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] constexpr auto await_suspend(auto parentCoro) noexcept
        {
            auto* coroTask = reinterpret_cast<CoroTaskT*>(this);

            auto& parentPromise = parentCoro.promise();
            auto  hdl           = coroTask->_hdl;
            auto& promise       = hdl.promise();

            parentPromise.child  = std::addressof(promise);
            promise.parent       = std::addressof(parentPromise);
            promise.stopSource   = parentPromise.StopSource();
            promise.pauseHandler = parentPromise.pauseHandler;
            return hdl;
        }
    };

    template <typename ReturnValueT, typename CoroTaskT>
    class AwaiterValue : private AwaiterBase<CoroTaskT>
    {
    protected:
        AwaiterValue() = default;

    public:
        using AwaiterBase<CoroTaskT>::await_ready;
        using AwaiterBase<CoroTaskT>::await_suspend;

        [[nodiscard]] constexpr ReturnValueT await_resume() noexcept
        {
            auto* coroTask = static_cast<CoroTaskT*>(this);
            return coroTask->_hdl.promise().ReturnValue();
        }
    };

    template <typename CoroTaskT>
    class AwaiterValue<void, CoroTaskT> : private AwaiterBase<CoroTaskT>
    {
    protected:
        AwaiterValue() = default;

    public:
        using AwaiterBase<CoroTaskT>::await_ready;
        using AwaiterBase<CoroTaskT>::await_suspend;

        constexpr void await_resume() noexcept { }
    };

} // namespace tinycoro

#endif // TINY_CORO_TASK_AWAITER_HPP