#ifndef __TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__
#define __TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__

#include <coroutine>
#include <concepts>
#include <variant>

#include "Common.hpp"
#include "Exception.hpp"
#include "PauseHandler.hpp"
#include "StaticStorage.hpp"
#include "DynamicStorage.hpp"

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

        using StaticStorageT = detail::StaticStorage<detail::ICoroHandleBridge, sizeof(UniversalBridgeT), detail::ICoroHandleBridge>;
        using DynamicStorageT = detail::DynamicStorage<detail::ICoroHandleBridge>;

    public:
        PackedCoroHandle() = default;

        template <typename PromiseT, template <typename> class HandleT>
            requires concepts::CoroHandle<PromiseT, HandleT, UniversalBridgeT>
        PackedCoroHandle(HandleT<PromiseT> hdl)
        {
            PimplConstruct<PromiseT>(hdl);
        }

        template <typename PromiseT, template <typename> class HandleT>
            requires concepts::CoroHandle<PromiseT, HandleT, UniversalBridgeT>
        PackedCoroHandle& operator=(HandleT<PromiseT> hdl)
        {
            // possible PromiseT type change. reset required
            PimplReset();

            PimplConstruct<PromiseT>(hdl);
            return *this;
        }

        // disable copy and move
        PackedCoroHandle(PackedCoroHandle&&) = delete;

        PackedCoroHandle(std::nullptr_t) { }

        PackedCoroHandle& operator=(std::nullptr_t)
        {
            PimplReset();
            return *this;
        }

        [[nodiscard]] PackedCoroHandle& Child()
        {
            if (Empty())
            {
                throw CoroHandleException("No Child coroutine");
            }
            return PimplChild();
        }

        [[nodiscard]] PackedCoroHandle& Parent()
        {
            if (Empty())
            {
                throw CoroHandleException("No Parent coroutine");
            }
            return PimplParent();
        }

        [[nodiscard]] std::coroutine_handle<> Handle()
        {
            if (Empty() == false)
            {
                return PimplHandle();
            }
            return std::noop_coroutine();
        }

        [[nodiscard]] bool Done() const
        {
            if (Empty() == false)
            {
                return PimplDone();
            }
            return true;
        }

        [[nodiscard]] auto ResumeState() const
        {
            if (Empty() == false)
            {
                return PimplResumeState();
            }

            return ETaskResumeState::DONE;
        }

        [[nodiscard]] ETaskResumeState Resume()
        {
            if (Empty() == false)
            {
                if (auto hdl = PimplHandle(); hdl)
                {
                    hdl.resume();
                }
            }

            return ResumeState();
        }

        void ReleaseHandle()
        {
            if (Empty() == false)
            {
                return std::visit([](auto& p) { return p->ReleaseHandle(); }, _pimpl);
            }
        }

        operator bool() const noexcept { return Empty() == false && PimplHandle(); }

    private:

        [[nodiscard]] constexpr bool Empty() const noexcept
        {
            return std::visit([](const auto& p) { return p == nullptr; }, _pimpl);
        }

        template<typename PromiseT, typename HandleT>
        void PimplConstruct(HandleT hdl)
        {
            using BridgeImplT = detail::CoroHandleBridgeImpl<PromiseT>;

            if constexpr (sizeof(BridgeImplT) <= StaticStorageT::size)
            {
                if(std::holds_alternative<StaticStorageT>(_pimpl) == false)
                {
                    _pimpl = StaticStorageT{};
                }
            }
            else
            {
                if(std::holds_alternative<DynamicStorageT>(_pimpl) == false)
                {
                    _pimpl = DynamicStorageT{};
                }
            }

            std::visit([&hdl]<typename T>(T& p) { 
                
                if constexpr (std::same_as<T, StaticStorageT> && sizeof(BridgeImplT) <= StaticStorageT::size)
                {
                    p.template Construct<BridgeImplT>(hdl);
                }
                else if constexpr(std::same_as<T, DynamicStorageT>)
                {
                    p.template Construct<BridgeImplT>(hdl);
                }
            }, _pimpl);
        }

        void PimplReset()
        {
            std::visit([](auto& p) { p.reset(); }, _pimpl);
        }

        bool PimplDone() const
        {
            return std::visit([](auto& p) { return p->Done(); }, _pimpl);
        }

        ETaskResumeState PimplResumeState() const
        {
            return std::visit([](auto& p) { return p->ResumeState(); }, _pimpl);
        }

        std::coroutine_handle<> PimplHandle() const
        {
            return std::visit([](auto& p) { return p->Handle(); }, _pimpl);
        }

        PackedCoroHandle& PimplChild()
        {
            return std::visit([](auto& p) -> decltype(auto) { return p->Child(); }, _pimpl);
        }

        PackedCoroHandle& PimplParent()
        {
            return std::visit([](auto& p) -> decltype(auto) { return p->Parent(); }, _pimpl);
        }

        std::variant<StaticStorageT, DynamicStorageT> _pimpl;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP__