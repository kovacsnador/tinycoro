#ifndef __TINY_CORO_PROMISE_FUTURE_HPP__
#define __TINY_CORO_PROMISE_FUTURE_HPP__

#include <variant>
#include <exception>
#include <stdexcept>
#include <memory>

namespace tinycoro {

    struct FutureException : std::runtime_error
    {
        using BaseT = std::runtime_error;
        using BaseT::BaseT;
    };

    namespace detail {

        template <typename ValueT>
        struct SharedState
        {
            enum struct EState : int32_t
            {
                UNSET         = 0,
                SETTING       = 1,
                VALUE_SET     = 2,
                EXCEPTION_SET = 3,
                USED          = 4
            };

            using value_type = std::variant<std::exception_ptr, ValueT>;

            SharedState() = default;

            // disallow copy and move
            SharedState(SharedState&&) = delete;

            auto get() -> ValueT
            {
                auto expected = _state.load(std::memory_order_relaxed);
                while (expected < EState::VALUE_SET)
                {
                    // wait until the state is UNSET or SETTING
                    _state.wait(expected);
                    expected = _state.load(std::memory_order_relaxed);
                }

                // try to get the exclusive access
                // to the value, to be able to set it
                if (_state.compare_exchange_strong(expected, EState::USED, std::memory_order_release, std::memory_order_relaxed) == false)
                {
                    throw FutureException{"Future object is already used!"};
                }

                if (expected == EState::EXCEPTION_SET)
                {
                    std::rethrow_exception(std::get<std::exception_ptr>(std::move(_value)));
                }

                return std::get<ValueT>(std::move(_value));
            }

            template <typename... Args>
            void set_value(Args&&... args)
            {
                set<ValueT>(std::forward<Args>(args)...);
            }

            template <typename... Args>
            void set_exception(Args&&... args)
            {
                set<std::exception_ptr>(std::forward<Args>(args)...);
            }

            template <typename T, typename... Args>
            void set(Args&&... args)
            {
                auto expected{EState::UNSET};
                if (_state.compare_exchange_strong(expected, EState::SETTING, std::memory_order_release, std::memory_order_relaxed) == false)
                {
                    throw FutureException{"Future is already set!"};
                }

                // we are the first ones to setting the value
                _value.template emplace<T>(std::forward<Args>(args)...);

                auto state = [] {
                    if constexpr (std::same_as<T, std::exception_ptr>)
                    {
                        return EState::EXCEPTION_SET;
                    }
                    else
                    {
                        return EState::VALUE_SET;
                    }
                };

                _state.store(state(), std::memory_order_release);

                // we are notify only one waiter
                // this is for performance reason
                //
                // in our scenario this is perfectly safe
                // but it's not recomended for other usecases.
                _state.notify_one();
            }

            value_type          _value{};
            std::atomic<EState> _state{EState::UNSET};
        };

    } // namespace detail

    namespace unsafe {

        template <typename T>
        class Future;

        // This is a lightweight/fast but unsafe implementation of a Promise-Future idiom.
        //
        // It only fullfills the requirements of tinycoro.
        template <typename ValueT>
        class Promise
        {
            using sharedState_t = detail::SharedState<ValueT>;
            using future_t = Future<ValueT>;

        public:
            Promise() = default;

            Promise(Promise&& other) = default;

            template <typename... Args>
            void set_value(Args&&... args)
            {
                assert(_sharedState);
                _sharedState->set_value(std::forward<Args>(args)...);
            }

            template <typename... Args>
            void set_exception(Args&&... args)
            {
                assert(_sharedState);
                _sharedState->set_exception(std::forward<Args>(args)...);
            }

            auto get_future()
            {
                auto sharedState = std::make_unique<sharedState_t>();
                _sharedState     = sharedState.get();
                return future_t{std::move(sharedState)};
            }

        private:
            sharedState_t* _sharedState{nullptr};
        };

        // This is a lightweight/fast but unsafe implementation of a Promise-Future idiom.
        //
        // It only fullfills the requirements of tinycoro.
        template <typename ValueT>
        class Future
        {
            friend Promise<ValueT>;

            using sharedState_t = detail::SharedState<ValueT>;

        public:
            Future() = default;

            Future(Future&& other) = default;

            auto get() -> ValueT
            {
                assert(_sharedState);
                return _sharedState->get();
            }

        private:
            Future(std::unique_ptr<sharedState_t>&& sharedState)
            : _sharedState{std::move(sharedState)}
            {
            }

            std::unique_ptr<sharedState_t> _sharedState{};
        };

    } // namespace unsafe
} // namespace tinycoro

#endif //!__TINY_CORO_PROMISE_FUTURE_HPP__