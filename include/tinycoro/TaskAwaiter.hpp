// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_TASK_AWAITER_HPP
#define TINY_CORO_TASK_AWAITER_HPP

namespace tinycoro {
    template <typename CoroTaskT>
    struct AwaiterBase
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] constexpr auto await_suspend(auto parentCoro) noexcept
        {
            auto* coroTask = static_cast<CoroTaskT*>(this);

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
    class AwaiterValue : public AwaiterBase<CoroTaskT>
    {
    protected:
        AwaiterValue() = default;

    public:
        [[nodiscard]] constexpr ReturnValueT await_resume() noexcept
        {
            auto* coroTask = static_cast<CoroTaskT*>(this);
            return coroTask->_hdl.promise().value();
        }
    };

    template <typename CoroTaskT>
    class AwaiterValue<void, CoroTaskT> : public AwaiterBase<CoroTaskT>
    {
    protected:
        AwaiterValue() = default;

    public:
        constexpr void await_resume() const noexcept { }
    };

} // namespace tinycoro

#endif // TINY_CORO_TASK_AWAITER_HPP