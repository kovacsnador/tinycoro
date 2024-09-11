#ifndef __TINY_CORO_CORO_FUTURE_HPP__
#define __TINY_CORO_CORO_FUTURE_HPP__

#include <concepts>
#include <atomic>
#include <memory>
#include <optional>
#include <cassert>
#include <variant>
#include <mutex>

#include "Common.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename To, typename From>
        concept Assignable = requires (To t, From f) {
            { t = f };
        } || requires (To t, From&& f) {
            { t = std::move(f) };
        };

    } // namespace concepts

    struct AssociatedStateBase
    {
        enum class EState
        {
            WAITING,
            VALID,
            DONE
        };

        void Wait()
        {
            _state.wait(EState::WAITING);

            EState state = EState::VALID;
            _state.compare_exchange_strong(state, EState::DONE);
        }

        void Notify()
        {
            _state.store(EState::VALID, std::memory_order_release);
            _state.notify_all();
        }

        [[nodiscard]] bool Ready() const noexcept
        {
            return _state.load() == EState::WAITING;
        }

        [[nodiscard]] bool Valid() const noexcept { return _state.load() != EState::DONE; }

    
        std::atomic<EState> _state{EState::WAITING};
    };

    template <typename ValueT>
    struct AssociatedState : private AssociatedStateBase
    {
    private:
        using value_type = std::variant<std::monostate, ValueT, std::exception_ptr>;

    public:
        using AssociatedStateBase::Valid;
        using AssociatedStateBase::Ready;

        template <typename T>
            requires concepts::Assignable<value_type, T>
        void Set(T&& val)
        {
            if (Ready())
            {
                {
                    std::scoped_lock lock{_mtx};
                    // set only once
                    _value = std::forward<T>(val);
                }

                Notify();
            }
            else
            {
                throw AssociatedStateStatisfiedException("Future can be set only once!");
            }
        }

        auto&& Get()
        {
            Wait();

            if (std::holds_alternative<std::exception_ptr>(_value))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(_value));
            }

            return std::move(std::get<ValueT>(_value));
        }

    private:
        mutable std::mutex  _mtx;
        value_type          _value;
    };

    template <>
    struct AssociatedState<void> : private AssociatedStateBase
    {   
        using AssociatedStateBase::Valid;
        using AssociatedStateBase::Ready;

        template <typename T = std::monostate>
        void Set(T&& value = {})
        {
            if (Ready())
            {
                if constexpr (std::same_as<std::exception_ptr, std::decay_t<T>>)
                {
                    std::scoped_lock lock{_mtx};
                    _exception = std::forward<T>(value);
                }

                Notify();
            }
            else
            {
                throw AssociatedStateStatisfiedException("Future can be set only once!");
            }
        }

        void Get()
        {
            Wait();

            if (std::scoped_lock lock{_mtx}; _exception)
            {
                std::rethrow_exception(_exception);
            }
        }

    private:

        mutable std::mutex _mtx;
        std::exception_ptr _exception;
    };

    template <typename ValueT>
    struct Future
    {
        using value_type = ValueT;

        Future(std::shared_ptr<AssociatedState<ValueT>> state)
        : _associatedState{std::move(state)}
        {
            assert(_associatedState);
        }

        [[nodiscard]] auto&& get()
        {
            assert(_associatedState);

            return _associatedState->Get();
        }

        [[nodiscard]] bool valid() const { return _associatedState->Valid(); }

    private:
        std::shared_ptr<AssociatedState<ValueT>> _associatedState;
    };

    template <>
    struct Future<void>
    {
        using value_type = void;

        Future(std::shared_ptr<AssociatedState<void>> state)
        : _associatedState{std::move(state)}
        {
            assert(_associatedState);
        }

        void get() const
        {
            assert(_associatedState);

            _associatedState->Get();
        }

        [[nodiscard]] bool valid() const { return _associatedState->Valid(); }

    private:
        std::shared_ptr<AssociatedState<void>> _associatedState;
    };

    template <typename ValueT>
    struct FutureState
    {
        FutureState()
        : _associatedState{std::make_shared<AssociatedState<ValueT>>()}
        {
        }

        ~FutureState()
        {
            if (_associatedState && _associatedState->Ready())
            {
                try
                {
                    throw FutureStateException("FutureState destroyed but is not done yet.");
                }
                catch (const std::exception&)
                {
                    _associatedState->Set(std::current_exception());
                }
            }
        }

        FutureState(FutureState&&)            = default;
        FutureState& operator=(FutureState&&) = default;

        auto get_future() { return Future<ValueT>{_associatedState}; }

        template <typename... Ts>
            requires (!std::same_as<void, ValueT>)
        void set_value(Ts&&... val)
        {
            _associatedState->Set(std::forward<Ts>(val)...);
        }

        void set_value() requires std::same_as<void, ValueT>
        {
            _associatedState->Set();
        }

        template <typename... Ts>
        void set_exception(Ts&&... val)
        {
            _associatedState->Set(std::forward<Ts>(val)...);
        }

        [[nodiscard]] bool valid() const { return _associatedState->Valid(); }

    private:
        std::shared_ptr<AssociatedState<ValueT>> _associatedState;
    };

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_FUTURE_HPP__
