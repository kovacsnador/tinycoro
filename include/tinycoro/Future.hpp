#ifndef __TINY_CORO_CORO_FUTURE_HPP__
#define __TINY_CORO_CORO_FUTURE_HPP__

#include <concepts>
#include <atomic>
#include <memory>
#include <optional>
#include <cassert>

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

    template <typename ValueT>
    struct AssociatedState
    {
    private:
        using value_type = std::variant<std::monostate, ValueT, std::exception_ptr>;

    public:
        template <typename T>
            requires concepts::Assignable<value_type, T>
        void Set(T&& val)
        {
            if (Valid())
            {
                {
                    std::scoped_lock lock{_mtx};
                    // set only once
                    _value = std::forward<T>(val);
                }

                _done.store(true, std::memory_order_release);
                _done.notify_all();
            }
            else
            {
                throw AssociatedStateStatisfiedException("Future can be set only once!");
            }
        }

        auto&& Get()
        {
            _done.wait(false);

            if (std::holds_alternative<std::exception_ptr>(_value))
            {
                std::rethrow_exception(std::get<std::exception_ptr>(_value));
            }

            return std::get<ValueT>(_value);
        }

        [[nodiscard]] bool Valid() const noexcept { return !_done.load(); }

    private:
        mutable std::mutex _mtx;
        value_type         _value;
        std::atomic<bool>  _done{false};
    };

    template <>
    struct AssociatedState<void>
    {
        AssociatedState() = default;

        template <typename T = std::monostate>
        void Set(T&& t = {})
        {
            if (Valid())
            {
                if constexpr (std::same_as<std::exception_ptr, std::decay_t<T>>)
                {
                    std::scoped_lock lock{_mtx};
                    _exception = std::forward<T>(t);
                }

                _done.store(true, std::memory_order_release);
                _done.notify_all();
            }
            else
            {
                throw AssociatedStateStatisfiedException("Future can be set only once!");
            }
        }

        void Get() const
        {
            _done.wait(false);

            if (std::scoped_lock lock{_mtx}; _exception)
            {
                std::rethrow_exception(_exception);
            }
        }

        [[nodiscard]] bool Valid() const noexcept { return !_done.load(); }

    private:
        mutable std::mutex _mtx;
        std::exception_ptr _exception;

        std::atomic<bool> _done{false};
    };

    template <typename ValueT>
    struct Future
    {
        using value_type = ValueT;

        Future(std::shared_ptr<AssociatedState<ValueT>> state)
        : _state{std::move(state)}
        {
            assert(_state);
        }

        [[nodiscard]] auto&& get()
        {
            assert(_state);

            return _state->Get();
        }

        [[nodiscard]] bool valid() const { return _state->Valid(); }

    private:
        std::shared_ptr<AssociatedState<ValueT>> _state;
    };

    template <>
    struct Future<void>
    {
        using value_type = void;

        Future(std::shared_ptr<AssociatedState<void>> state)
        : _state{std::move(state)}
        {
            assert(_state);
        }

        void get() const
        {
            assert(_state);

            _state->Get();
        }

        [[nodiscard]] bool valid() const { return _state->Valid(); }

    private:
        std::shared_ptr<AssociatedState<void>> _state;
    };

    template <typename ValueT>
    struct FutureState
    {
        FutureState()
        : _state{std::make_shared<AssociatedState<ValueT>>()}
        {
        }

        ~FutureState()
        {
            if (_state && _state->Valid())
            {
                try
                {
                    throw FutureStateException("FutureState destroyed but is not done yet.");
                }
                catch (const std::exception&)
                {
                    _state->Set(std::current_exception());
                }
            }
        }

        FutureState(FutureState&&)            = default;
        FutureState& operator=(FutureState&&) = default;

        auto get_future() { return Future<ValueT>{_state}; }

        template <typename... Ts>
        void set_value(Ts&&... val)
        {
            _state->Set(std::forward<Ts>(val)...);
        }

        template <typename... Ts>
        void set_exception(Ts&&... val)
        {
            _state->Set(std::forward<Ts>(val)...);
        }

        [[nodiscard]] bool valid() const { return _state->Valid(); }

    private:
        std::shared_ptr<AssociatedState<ValueT>> _state;
    };

} // namespace tinycoro

#endif // !__TINY_CORO_CORO_FUTURE_HPP__