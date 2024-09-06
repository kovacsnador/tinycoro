#ifndef __TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__
#define __TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__

#include <coroutine>
#include <concepts>

#include "Common.hpp"
#include "Exception.hpp"
#include "PauseHandler.hpp"
#include "StaticStorage.hpp"

namespace tinycoro {

    struct PackedCoroHandle;

    namespace detail {

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
                    if constexpr (requires { _hdl.promise().pauseHandler; })
                    {
                        if (_hdl.promise().pauseHandler->IsPaused())
                        {
                            return ECoroResumeState::PAUSED;
                        }
                    }
                    return ECoroResumeState::SUSPENDED;
                }
                return ECoroResumeState::DONE;
            }

            void ReleaseHandle() override { _hdl = nullptr; }

        private:
            std::coroutine_handle<PromiseT> _hdl;
        };

        template <>
        struct CoroHandleBridgeImpl<void> : public ICoroHandleBridge
        {
            std::coroutine_handle<void> _hdl;
        };

    } // namespace detail

    struct [[nodiscard]] PackedCoroHandle
    {
    private:
        using UniversalBridgeT = detail::CoroHandleBridgeImpl<void>;

    public:
        PackedCoroHandle() = default;

        template <typename PromiseT, template <typename> class HandleT>
            requires std::constructible_from<detail::CoroHandleBridgeImpl<PromiseT>, HandleT<PromiseT>>
            && (std::alignment_of_v<detail::CoroHandleBridgeImpl<PromiseT>> == std::alignment_of_v<UniversalBridgeT>)
        PackedCoroHandle(HandleT<PromiseT> hdl)
        : _bridge{std::type_identity<detail::CoroHandleBridgeImpl<PromiseT>>{}, hdl}
        {
        }

        template <typename PromiseT, template <typename> class HandleT>
            requires std::constructible_from<detail::CoroHandleBridgeImpl<PromiseT>, HandleT<PromiseT>>
        PackedCoroHandle& operator=(HandleT<PromiseT> hdl)
        {
            _bridge = decltype(_bridge){std::type_identity<detail::CoroHandleBridgeImpl<PromiseT>>{}, hdl};
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
        StaticStorage<detail::ICoroHandleBridge, sizeof(UniversalBridgeT), UniversalBridgeT> _bridge;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__