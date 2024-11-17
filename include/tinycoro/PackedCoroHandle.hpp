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
            virtual ETaskResumeState        ResumeState() const = 0;
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

            ETaskResumeState ResumeState() const override
            {
                if (Done() == false)
                {
                    if constexpr (requires { _hdl.promise().pauseHandler; })
                    {
                        if (_hdl.promise().pauseHandler && _hdl.promise().pauseHandler->IsPaused())
                        {
                            return ETaskResumeState::PAUSED;
                        }
                    }
                    return ETaskResumeState::SUSPENDED;
                }
                return ETaskResumeState::DONE;
            }

            void ReleaseHandle() override { _hdl = nullptr; }

        private:
            std::coroutine_handle<PromiseT> _hdl;
        };

        template <>
        struct CoroHandleBridgeImpl<void> : public ICoroHandleBridge
        {
            std::coroutine_handle<> _hdl;
        };

    } // namespace detail

    namespace concepts {

        template <typename PromiseT, template <typename> class HandleT, typename UniversalBridgeT>
        concept CoroHandle = std::constructible_from<detail::CoroHandleBridgeImpl<PromiseT>, HandleT<PromiseT>>
            && (std::alignment_of_v<detail::CoroHandleBridgeImpl<PromiseT>> == std::alignment_of_v<UniversalBridgeT>)
            && (sizeof(detail::CoroHandleBridgeImpl<PromiseT>) == sizeof(UniversalBridgeT));

    } // namespace concepts

    struct [[nodiscard]] PackedCoroHandle
    {
    private:
        using UniversalBridgeT = detail::CoroHandleBridgeImpl<void>;

    public:
        PackedCoroHandle() = default;

        template <typename PromiseT, template <typename> class HandleT>
            requires concepts::CoroHandle<PromiseT, HandleT, UniversalBridgeT>
        PackedCoroHandle(HandleT<PromiseT> hdl)
        {
            _pimpl.Construct<detail::CoroHandleBridgeImpl<PromiseT>>(hdl);
        }

        template <typename PromiseT, template <typename> class HandleT>
            requires concepts::CoroHandle<PromiseT, HandleT, UniversalBridgeT>
        PackedCoroHandle& operator=(HandleT<PromiseT> hdl)
        {
            // possible PromiseT type changes. reset required
            _pimpl.reset();
            _pimpl.Construct<detail::CoroHandleBridgeImpl<PromiseT>>(hdl);

            return *this;
        }

        // disable copy and move
        PackedCoroHandle(PackedCoroHandle&&) = delete;

        PackedCoroHandle(std::nullptr_t) { }

        PackedCoroHandle& operator=(std::nullptr_t)
        {
            _pimpl.reset();
            return *this;
        }

        [[nodiscard]] PackedCoroHandle& Child()
        {
            if (_pimpl == nullptr)
            {
                throw CoroHandleException("No Child coroutine");
            }
            return _pimpl->Child();
        }

        [[nodiscard]] PackedCoroHandle& Parent()
        {
            if (_pimpl == nullptr)
            {
                throw CoroHandleException("No Parent coroutine");
            }
            return _pimpl->Parent();
        }

        [[nodiscard]] std::coroutine_handle<> Handle()
        {
            if (_pimpl)
            {
                return _pimpl->Handle();
            }
            return std::noop_coroutine();
        }

        [[nodiscard]] bool Done() const
        {
            if (_pimpl)
            {
                return _pimpl->Done();
            }
            return true;
        }

        [[nodiscard]] auto ResumeState() const
        {
            if (_pimpl)
            {
                return _pimpl->ResumeState();
            }

            return ETaskResumeState::DONE;
        }

        [[nodiscard]] ETaskResumeState Resume()
        {
            if (_pimpl)
            {
                if (auto hdl = _pimpl->Handle(); hdl)
                {
                    hdl.resume();
                }
            }

            return ResumeState();
        }

        void ReleaseHandle()
        {
            if (_pimpl)
            {
                return _pimpl->ReleaseHandle();
            }
        }

        operator bool() const noexcept { return _pimpl != nullptr && _pimpl->Handle(); }

    private:
        detail::StaticStorage<detail::ICoroHandleBridge, sizeof(UniversalBridgeT), UniversalBridgeT> _pimpl;

        //std::unique_ptr<detail::ICoroHandleBridge> _pimpl;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__