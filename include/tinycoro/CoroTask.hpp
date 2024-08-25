#ifndef __TINY_CORO_CORO_TASK_HPP__
#define __TINY_CORO_CORO_TASK_HPP__

#include <chrono>
#include <coroutine>
#include <optional>
#include <cstddef>

#include "Common.hpp"
#include "StaticStorage.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
        concept Pausable = requires (T t) {
            { t.paused = true };
            requires std::regular_invocable<decltype(t.pauseResume)>;
        };

    } // namespace concepts

    struct [[nodiscard]] PackedCoroHandle
    {
    private:
        struct ICoroHandleBridge
        {
            virtual ~ICoroHandleBridge() = default;

            virtual PackedCoroHandle&       Child()             = 0;
            virtual PackedCoroHandle&       Parent()            = 0;
            virtual bool                    Done() const        = 0;
            virtual ECoroResumeState        ResumeState() const = 0;
            virtual std::coroutine_handle<> Handle()            = 0;
            virtual std::coroutine_handle<> Handle() const      = 0;
            virtual void                    ReleaseHandle()     = 0;
        };

        template <typename PromiseT>
        struct CoroHandleBridgeImpl : public ICoroHandleBridge
        {
            CoroHandleBridgeImpl(std::coroutine_handle<PromiseT> h)
            : _hdl{h}
            {
            }

            PackedCoroHandle& Child() override { return _hdl.promise().child; }

            PackedCoroHandle& Parent() override { return _hdl.promise().parent; }

            std::coroutine_handle<> Handle() override { return _hdl; }

            std::coroutine_handle<> Handle() const override { return _hdl; }

            bool Done() const override
            {
                if (_hdl)
                {
                    return _hdl.done();
                }
                return true;
            }

            ECoroResumeState ResumeState() const override
            {
                if (Done() == false)
                {
                    if (_hdl.promise().paused)
                    {
                        return ECoroResumeState::PAUSED;
                    }
                    return ECoroResumeState::SUSPENDED;
                }
                return ECoroResumeState::DONE;
            }

            void ReleaseHandle() override { _hdl = nullptr; }

        private:
            std::coroutine_handle<PromiseT> _hdl;
        };

        using UniversalBridgeT = CoroHandleBridgeImpl<void>;

    public:
        PackedCoroHandle() = default;

        template <typename PromiseT, template <typename> class HandleT>
            requires std::constructible_from<CoroHandleBridgeImpl<PromiseT>, HandleT<PromiseT>>
            && (std::alignment_of_v<CoroHandleBridgeImpl<PromiseT>> == std::alignment_of_v<UniversalBridgeT>)
        PackedCoroHandle(HandleT<PromiseT> hdl)
        : _bridge{std::type_identity<CoroHandleBridgeImpl<PromiseT>>{}, hdl}
        {
        }

        template <typename PromiseT, template <typename> class HandleT>
            requires std::constructible_from<CoroHandleBridgeImpl<PromiseT>, HandleT<PromiseT>>
        PackedCoroHandle& operator=(HandleT<PromiseT> hdl)
        {
            _bridge = decltype(_bridge){std::type_identity<CoroHandleBridgeImpl<PromiseT>>{}, hdl};
            return *this;
        }

        PackedCoroHandle(std::nullptr_t) { }

        PackedCoroHandle& operator=(std::nullptr_t)
        {
            _bridge.reset();
            return *this;
        }

        [[nodiscard]] PackedCoroHandle& Child()
        {
            if (_bridge == nullptr)
            {
                throw CoroHandleException("No Child coroutine");
            }
            return _bridge->Child();
        }

        [[nodiscard]] PackedCoroHandle& Parent()
        {
            if (_bridge == nullptr)
            {
                throw CoroHandleException("No Parent coroutine");
            }
            return _bridge->Parent();
        }

        [[nodiscard]] std::coroutine_handle<> Handle()
        {
            if (_bridge)
            {
                return _bridge->Handle();
            }
            return std::noop_coroutine();
        }

        [[nodiscard]] bool Done() const
        {
            if (_bridge)
            {
                return _bridge->Done();
            }
            return true;
        }

        [[nodiscard]] auto ResumeState() const
        {
            if (_bridge)
            {
                return _bridge->ResumeState();
            }

            return ECoroResumeState::DONE;
        }

        [[nodiscard]] ECoroResumeState Resume()
        {
            if (_bridge)
            {
                if (auto hdl = _bridge->Handle(); hdl)
                {
                    hdl.resume();
                }
            }

            return ResumeState();
        }

        void ReleaseHandle()
        {
            if (_bridge)
            {
                return _bridge->ReleaseHandle();
            }
        }

        operator bool() const noexcept { return _bridge != nullptr && _bridge->Handle(); }

    private:
        StaticStorage<ICoroHandleBridge, sizeof(UniversalBridgeT), UniversalBridgeT> _bridge;
    };

    struct FinalAwaiter
    {
        [[nodiscard]] bool await_ready() const noexcept { return false; }

        [[nodiscard]] std::coroutine_handle<> await_suspend(auto hdl) noexcept
        {
            auto& promise = hdl.promise();

            if (promise.parent)
            {
                promise.parent.Child().ReleaseHandle();
                promise.parent.Child() = nullptr;
                return promise.parent.Handle();
            }

            return std::noop_coroutine();
        }

        void await_resume() const noexcept { }
    };

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
            /*if (stopSource.stop_requested())
            {
                return ECoroResumeState::STOPPED;
            }*/

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

        void pause(std::regular_invocable auto resumerCallback)
        {
            if constexpr (requires { requires concepts::Pausable<decltype(_hdl.promise())>; })
            {
                _hdl.promise().pauseResume = resumerCallback;
            }
        }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            _source = std::forward<T>(arg);
            _hdl.promise().stopToken = _source.get_token();
        }

    private:
        void destroy()
        { 
            _source.request_stop();
        }

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
        friend class AwaiterBase<CoroTask<ReturnValueT, PromiseT, AwaiterT>>;
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

        void pause(std::regular_invocable auto resumerCallback)
        {
            if constexpr (requires { requires concepts::Pausable<decltype(_hdl.promise())>; })
            {
                _hdl.promise().pauseResume = resumerCallback;
            }
        }

        [[nodiscard]] auto task_view() const noexcept { return CoroTaskView<promise_type, awaiter_type, CoroResumerT>{_hdl, _source}; }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            _source = std::forward<T>(arg);
            _hdl.promise().stopToken = _source.get_token();
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

    template <typename ValueT>
    struct PromiseReturnValue
    {
        using value_type = ValueT;

        template <typename U>
        void return_value(U&& v)
        {
            if constexpr (requires { _value = std::forward<U>(v); })
            {
                _value = std::forward<U>(v);
            }
            else
            {
                // to support aggregate initialization
                _value = decltype(_value){std::in_place, std::forward<U>(v)};
            }
        }

        auto&& ReturnValue() { return std::move(_value.value()); }

    private:
        std::optional<value_type> _value{};
    };

    template <>
    struct PromiseReturnValue<void>
    {
        using value_type = void;

        void return_void() { }
    };

    template <typename ValueT, typename AwaiterT>
    struct PromiseYieldValue
    {
        using value_type = ValueT;

        template <typename U>
        auto yield_value(U&& v)
        {
            _value = std::forward<U>(v);
            return AwaiterT{};
        }

        const auto& YieldValue() const { return _value.value(); }

    private:
        std::optional<value_type> _value{};
    };

    template <typename PackedCoroHandleT>
    struct CoroHandleNode
    {
        PackedCoroHandleT parent;
        PackedCoroHandleT child;
    };

    template <typename FinalAwaiterT, typename StopTokenT = std::stop_token>
    struct PromiseBase : private CoroHandleNode<PackedCoroHandle>
    {
        using CoroHandleNode::child;
        using CoroHandleNode::parent;

        std::function<void()> pauseResume;
        bool                  paused{false};

        StopTokenT stopToken{};

        auto initial_suspend() { return std::suspend_always{}; }

        auto final_suspend() noexcept { return FinalAwaiterT{}; }

        void unhandled_exception() { std::rethrow_exception(std::current_exception()); }
    };

    template <typename ReturnerT>
    concept PromiseReturnerConcept = requires (ReturnerT r) {
        { r.return_void() };
    } || requires (ReturnerT r) {
        { r.return_value(std::declval<typename ReturnerT::value_type>()) };
    };

    template <typename YielderT>
    concept PromiseYielderConcept = requires (YielderT y) {
        { y.yield_value(std::declval<typename YielderT::value_type>()) };
    };

    template <typename... Types>
    struct Promise;

    template <typename FinalAwaiterT, PromiseReturnerConcept ReturnerT>
    struct Promise<FinalAwaiterT, ReturnerT> : public PromiseBase<FinalAwaiterT>, public ReturnerT
    {
        auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }
    };

    template <typename FinalAwaiterT, PromiseReturnerConcept ReturnerT, PromiseYielderConcept YielderT>
    struct Promise<FinalAwaiterT, ReturnerT, YielderT> : public PromiseBase<FinalAwaiterT>, public ReturnerT, public YielderT
    {
        auto get_return_object() { return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this); }
    };

    template <typename ReturnValueT>
    using Task = CoroTask<ReturnValueT, Promise<FinalAwaiter, PromiseReturnValue<ReturnValueT>>, AwaiterValue>;

} // namespace tinycoro

#endif //!__TINY_CORO_CORO_TASK_HPP__