#ifndef __TINY_CORO_CORO_TASK_HPP__
#define __TINY_CORO_CORO_TASK_HPP__

#include <chrono>
#include <coroutine>
#include <optional>
#include <cstddef>
#include <stop_token>

#include "Common.hpp"
#include "StaticStorage.hpp"
#include "PackedCoroHandle.hpp"
#include "Exception.hpp"
#include "PauseHandler.hpp"
#include "Promise.hpp"

namespace tinycoro {

    template <typename CoroTaskT>
    struct AwaiterBase
    {
        [[nodiscard]] constexpr bool await_ready() const noexcept { return false; }

        [[nodiscard]] constexpr auto await_suspend(auto parentCoro) noexcept
        {
            auto* coroTask = reinterpret_cast<CoroTaskT*>(this);

            auto hdl                   = coroTask->_hdl;
            parentCoro.promise().child = hdl;
            hdl.promise().parent       = parentCoro;
            hdl.promise().stopSource   = parentCoro.promise().stopSource;
            hdl.promise().pauseHandler = parentCoro.promise().pauseHandler;
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

        [[nodiscard]] constexpr auto&& await_resume() noexcept
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

    struct CoroResumer
    {
        ECoroResumeState operator()(auto coroHdl, [[maybe_unused]] const auto& stopSource)
        {
            if (stopSource.stop_requested() && coroHdl.promise().cancellable)
            {
                return ECoroResumeState::STOPPED;
            }

            PackedCoroHandle  hdl{coroHdl};
            PackedCoroHandle* hdlPtr = std::addressof(hdl);

            while (*hdlPtr && hdlPtr->Child() && hdlPtr->Child().Done() == false)
            {
                hdlPtr = std::addressof(hdlPtr->Child());
            }

            if (*hdlPtr && hdlPtr->Done() == false)
            {
                return hdlPtr->Resume();
            }

            return ECoroResumeState::DONE;
        }
    };

    template <typename PromiseT, typename AwaiterT, typename CoroResumerT = CoroResumer, typename StopSourceT = std::stop_source>
    struct CoroTaskView : private AwaiterT
    {
        using promise_type  = PromiseT;
        using coro_hdl_type = std::coroutine_handle<promise_type>;

        using AwaiterT::await_ready;
        using AwaiterT::await_resume;
        using AwaiterT::await_suspend;

        CoroTaskView(coro_hdl_type hdl, StopSourceT source)
        : _hdl{hdl}
        , _source{source}
        {
        }

        CoroTaskView(CoroTaskView&& other) noexcept
        : _hdl{std::exchange(other._hdl, nullptr)}
        , _source{std::exchange(other._source, StopSourceT{std::nostopstate})}
        {
        }

        CoroTaskView& operator=(CoroTaskView&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                destroy();
                _hdl    = std::exchange(other._hdl, nullptr);
                _source = std::exchange(other._source, StopSourceT{std::nostopstate});
            }
            return *this;
        }

        ~CoroTaskView() { destroy(); }

        [[nodiscard]] auto resume() { return std::invoke(_coroResumer, _hdl, _source); }

        void SetPauseHandler(concepts::PauseHandlerCb auto pauseResume)
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                using elementType = std::pointer_traits<decltype(_hdl.promise().pauseHandler)>::element_type;
                _hdl.promise().pauseHandler = std::make_shared<elementType>(pauseResume);
            }
        }

        bool IsPaused() const noexcept
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                return _hdl.promise().pauseHandler->IsPaused();
            }
            return false;
        }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            _source                   = std::forward<T>(arg);
            _hdl.promise().stopSource = _source;
        }

    private:
        void destroy() { _source.request_stop(); }

        [[no_unique_address]] CoroResumerT _coroResumer{};
        coro_hdl_type                      _hdl;
        StopSourceT                        _source{std::nostopstate};
    };

    template <typename ReturnValueT,
              typename PromiseT,
              template <typename, typename>
              class AwaiterT,
              typename CoroResumerT = CoroResumer,
              typename StopSourceT  = std::stop_source>
    struct CoroTask : private AwaiterT<ReturnValueT, CoroTask<ReturnValueT, PromiseT, AwaiterT>>
    {
        friend struct AwaiterBase<CoroTask<ReturnValueT, PromiseT, AwaiterT>>;
        friend class AwaiterT<ReturnValueT, CoroTask<ReturnValueT, PromiseT, AwaiterT>>;

        using awaiter_type = AwaiterT<ReturnValueT, CoroTask<ReturnValueT, PromiseT, AwaiterT>>;

        using awaiter_type::await_ready;
        using awaiter_type::await_resume;
        using awaiter_type::await_suspend;

        using promise_type  = PromiseT;
        using coro_hdl_type = std::coroutine_handle<promise_type>;

        template <typename... Args>
            requires std::constructible_from<coro_hdl_type, Args...>
        CoroTask(Args&&... args)
        : _hdl{std::forward<Args>(args)...}
        {
        }

        CoroTask(CoroTask&& other) noexcept
        : _hdl{std::exchange(other._hdl, nullptr)}
        , _source{std::exchange(other._source, StopSourceT{std::nostopstate})}
        {
        }

        CoroTask& operator=(CoroTask&& other) noexcept
        {
            if (std::addressof(other) != this)
            {
                destroy();
                _hdl    = std::exchange(other._hdl, nullptr);
                _source = std::exchange(other._source, StopSourceT{std::nostopstate});
            }
            return *this;
        }

        ~CoroTask() { destroy(); }

        [[nodiscard]] auto resume() { return std::invoke(_coroResumer, _hdl, _source); }

        void SetPauseHandler(concepts::PauseHandlerCb auto pauseResume)
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                using elementType = std::pointer_traits<decltype(_hdl.promise().pauseHandler)>::element_type;
                _hdl.promise().pauseHandler = std::make_shared<elementType>(pauseResume);
            }
        }

        bool IsPaused() const noexcept
        {
            if constexpr (requires { _hdl.promise().pauseHandler; } )
            {
                return _hdl.promise().pauseHandler->IsPaused();
            }
            return false;
        }

        [[nodiscard]] auto TaskView() const noexcept { return CoroTaskView<promise_type, awaiter_type, CoroResumerT>{_hdl, _source}; }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            _source                   = std::forward<T>(arg);
            _hdl.promise().stopSource = _source;
        }

    private:
        void destroy()
        {
            if (_hdl)
            {
                _source.request_stop();
                _hdl.destroy();
                _hdl = nullptr;
            }
        }

        [[no_unique_address]] CoroResumerT _coroResumer{};
        coro_hdl_type                      _hdl;
        StopSourceT                        _source{std::nostopstate};
    };

    template <typename ReturnValueT>
    using Task = CoroTask<ReturnValueT, Promise<ReturnValueT>, AwaiterValue>;

} // namespace tinycoro

#endif //!__TINY_CORO_CORO_TASK_HPP__