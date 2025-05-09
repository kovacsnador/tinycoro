#ifndef TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP
#define TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP

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
            virtual void                    Resume() const      = 0;
            virtual std::coroutine_handle<> Handle() const      = 0;
        };

        template <typename PromiseT>
        struct CoroHandleBridgeImpl : public ICoroHandleBridge
        {
            CoroHandleBridgeImpl(std::coroutine_handle<PromiseT> h)
            : _hdl{h}
            {
            }

            PackedCoroHandle& Child() override { return _hdl.promise().child; }

            std::coroutine_handle<> Handle() const override { return _hdl; }

            inline void Resume() const override
            {
                if(_hdl)
                {
                    _hdl.resume();
                }
            }

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

        template <size_t SIZE, typename PromiseT, template <typename> class HandleT>
        concept CoroHandle = std::constructible_from<detail::CoroHandleBridgeImpl<PromiseT>, HandleT<PromiseT>>
            && (std::alignment_of_v<detail::CoroHandleBridgeImpl<PromiseT>> == std::alignment_of_v<detail::ICoroHandleBridge>)
            && (sizeof(detail::CoroHandleBridgeImpl<PromiseT>) <= SIZE);

    } // namespace concepts

    struct [[nodiscard]] PackedCoroHandle
    {
    private:
        // Use TINYCORO_PACKED_CORO_HANDLE_SIZE to define the memory size used for the PackedCoroHandle implementation.
        // This size determines the storage allocation for coroutine handle bridges, which may need adjustment based on
        // specific system requirements or optimizations.
        // By default, it is set to the size of CoroHandleBridgeImpl<void>, aiming for the minimal memory necessary.
        // You can override it to increase the memory allocation as necessary by setting TINYCORO_PACKED_CORO_HANDLE_SIZE
        // as a compile-time configuration in your source file or project settings to accommodate larger or custom handles.
#ifdef TINYCORO_PACKED_CORO_HANDLE_SIZE
        constexpr static size_t SIZE = TINYCORO_PACKED_CORO_HANDLE_SIZE;
#else
        constexpr static size_t SIZE = sizeof(detail::CoroHandleBridgeImpl<void>);
#endif
        using StaticStorageT = detail::StaticStorage<detail::ICoroHandleBridge, SIZE, detail::ICoroHandleBridge>;

    public:
        PackedCoroHandle() = default;

        template <typename PromiseT, template <typename> class HandleT>
            requires concepts::CoroHandle<SIZE, PromiseT, HandleT>
        PackedCoroHandle(HandleT<PromiseT> hdl)
        {
            _pimpl.template Construct<detail::CoroHandleBridgeImpl<PromiseT>>(hdl);
        }

        template <typename PromiseT, template <typename> class HandleT>
            requires concepts::CoroHandle<SIZE, PromiseT, HandleT>
        PackedCoroHandle& operator=(HandleT<PromiseT> hdl)
        {
            // possible PromiseT type change. reset required
            _pimpl.reset();

            _pimpl.template Construct<detail::CoroHandleBridgeImpl<PromiseT>>(hdl);
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
            if (_pimpl.Empty())
            {
                throw CoroHandleException("No Child coroutine");
            }
            return _pimpl->Child();
        }

        [[nodiscard]] std::coroutine_handle<> Handle() const
        {
            if (_pimpl)
            {
                return _pimpl->Handle();
            }
            return std::noop_coroutine();
        }

        inline void Resume() const
        {
            if (_pimpl)
            {
                _pimpl->Resume();
            }
        }

        operator bool() const noexcept { return _pimpl && _pimpl->Handle(); }

    private:
        StaticStorageT _pimpl;
    };

} // namespace tinycoro

#endif // TINY_CORO_DETAIL_PACKED_CORO_HANDLE_HPP