#ifndef __TINY_CORO_PACKAGED_TASK_HPP__
#define __TINY_CORO_PACKAGED_TASK_HPP__

#include <concepts>
#include <variant>
#include <atomic>

#include "Common.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
		concept FutureState = ( requires(T f) { { f.set_value() }; } 
                                || requires(T f) { { f.set_value(f.get_future().get().value()) }; }) 
                            && requires(T f) { f.set_exception(std::exception_ptr{}); };

    } // namespace concepts

    namespace detail {

        enum class EPauseState
        {
            IDLE,
            PAUSED,

            NOTIFIED,
        };

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

            std::atomic<EPauseState> pauseState{EPauseState::IDLE};
        };

        template <concepts::IsCorouitneTask CoroT, concepts::FutureState FutureStateT>
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
                if (_exception)
                {
                    // if we had an exception we just set it
                    _futureState.set_exception(_exception);
                }
                else
                {
                    using futureValue_t = std::decay_t<decltype(std::declval<FutureStateT>().get_future().get())>;

                    if (_coro.IsDone())
                    {
                        // are we on a last suspend point?
                        // That means we had no cancellation before
                        if constexpr (requires {
                                          { _coro.await_resume() } -> std::same_as<void>;
                                      })
                        {
                            _futureState.set_value(VoidType{});
                        }
                        else
                        {
                            _futureState.set_value(_coro.await_resume());
                        }
                    }
                    else
                    {
                        // the task got cancelled
                        // we give back an empty optional
                        _futureState.set_value(futureValue_t{std::nullopt});
                    }
                }
            }

            void Resume() override
            {
                try
                {
                    // reset the pause state by every resume.
                    pauseState.store(EPauseState::IDLE, std::memory_order_relaxed);

                    _coro.Resume();
                }
                catch (...)
                {
                    _exception = std::current_exception();
                }
            }

            ETaskResumeState ResumeState() override
            {
                // value already set, the coroutine should be done
                if (_exception)
                {
                    // if there was an exception the task is done
                    return ETaskResumeState::DONE;
                }

                return _coro.ResumeState();
            }

            void SetPauseHandler(tinycoro::PauseHandlerCallbackT cb) override { _coro.SetPauseHandler(std::move(cb)); }

        private:
            CoroT              _coro;
            FutureStateT       _futureState;
            std::exception_ptr _exception{};
        };

        using SchedulableTask = std::unique_ptr<detail::ISchedulableBridged>;

        template <concepts::IsCorouitneTask CoroT, concepts::FutureState FutureStateT>
            requires (!std::is_reference_v<CoroT>)
        SchedulableTask MakeSchedulableTask(CoroT&& coro, FutureStateT futureState)
        {
            using BridgeType = detail::SchedulableBridgeImpl<CoroT, FutureStateT>;
            return std::make_unique<BridgeType>(std::move(coro), std::move(futureState));
        }

    } // namespace detail

} // namespace tinycoro

#endif //!__TINY_CORO_PACKAGED_TASK_HPP__