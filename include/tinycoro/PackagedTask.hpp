#ifndef __TINY_CORO_PACKAGED_TASK_HPP__
#define __TINY_CORO_PACKAGED_TASK_HPP__

#include <concepts>
#include <variant>

#include "Common.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
		concept FutureState = (requires(T f) { { f.set_value() }; } || requires(T f) { { f.set_value(f.get_future().get().value()) }; }) && requires(T f) { f.set_exception(std::exception_ptr{}); };

        template <typename T>
        concept CoroTask = std::move_constructible<T> && requires (T c) {
            { c.Resume() } -> std::same_as<void>;
            { c.IsDone() } -> std::same_as<bool>;
            { c.await_resume() };
            { c.ResumeState() } -> std::same_as<ETaskResumeState>;
            { c.SetPauseHandler([]{}) };
        };

    } // namespace concepts

    namespace detail {
        class ISchedulableBridged
        {
        public:
            ISchedulableBridged() = default;

            // disable copy and move
            ISchedulableBridged(ISchedulableBridged&&) = delete;

            virtual ~ISchedulableBridged() = default;

            virtual void             Resume()      = 0;
            virtual ETaskResumeState ResumeState() = 0;

            virtual void SetPauseHandler(tinycoro::PauseHandlerCallbackT) = 0;

            // need for double linkage
            ISchedulableBridged* prev{nullptr};
            ISchedulableBridged* next{nullptr};
        };

        template <concepts::CoroTask CoroT, concepts::FutureState FutureStateT>
        class SchedulableBridgeImpl : public ISchedulableBridged
        {
        public:
            SchedulableBridgeImpl(CoroT&& coro, FutureStateT&& futureState)
            : _coro{std::move(coro)}
            , _futureState{std::move(futureState)}
            {
            }

            // disable copy and move
            SchedulableBridgeImpl(SchedulableBridgeImpl&&) = delete;

            ~SchedulableBridgeImpl()
            {
                if (_needValueSet)
                {
                    if constexpr (requires {
                                      { _coro.await_resume() } -> std::same_as<void>;
                                  })
                    {
                        _futureState.set_value();
                    }
                    else
                    {
                        if(_coro.IsDone())
                        {
                            // are we on a last suspend point?
                            // That means we had no cancellation before
                            _futureState.set_value(_coro.await_resume());
                        }
                        else
                        {
                            using value_t = std::decay_t<decltype(std::declval<FutureStateT>().get_future().get())>;

                            // the task got cancelled
                            // we give back an empty optional
                            _futureState.set_value(value_t{std::nullopt});
                        }
                    }
                }
            }

            void Resume() override
            {
                try
                {
                    _coro.Resume();
                }
                catch (...)
                {
                    _futureState.set_exception(std::current_exception());
                    _needValueSet = false;
                }
            }

            ETaskResumeState ResumeState() override
            {
                // value already set, the coroutine should be done
                if (_needValueSet == false)
                {
                    return ETaskResumeState::DONE;
                }

                return _coro.ResumeState();
            }

            void SetPauseHandler(tinycoro::PauseHandlerCallbackT cb) override
            {
                _coro.SetPauseHandler(std::move(cb));
            }

        private:
            bool         _needValueSet{true};
            CoroT        _coro;
            FutureStateT _futureState;
        };
    } // namespace detail

    using SchedulableTask = std::unique_ptr<detail::ISchedulableBridged>;

    template <concepts::CoroTask CoroT, concepts::FutureState FutureStateT>
        requires (!std::is_reference_v<CoroT>)
    SchedulableTask MakeSchedulableTask(CoroT&& coro, FutureStateT futureState)
    {
        using BridgeType = detail::SchedulableBridgeImpl<CoroT, FutureStateT>;
        return std::make_unique<BridgeType>(std::move(coro), std::move(futureState));
    }

} // namespace tinycoro

#endif //!__TINY_CORO_PACKAGED_TASK_HPP__