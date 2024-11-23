#ifndef __TINY_CORO_PACKAGED_CORO_HPP__
#define __TINY_CORO_PACKAGED_CORO_HPP__

#include <concepts>
#include <variant>

#include "Common.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
		concept FutureState = (requires(T f) { { f.set_value() }; } || requires(T f) { { f.set_value(f.get_future().get()) }; }) && requires(T f) { f.set_exception(std::exception_ptr{}); };

        template <typename T>
        concept CoroTask = requires (T c) {
            { c.Resume() } -> std::same_as<ETaskResumeState>;
            { c.await_resume() };
            { c.IsPaused() } -> std::same_as<bool>;
        };

    } // namespace concepts

    struct PackagedTask
    {
        using PauseCallbackType = std::function<void()>;

    private:
        class ISchedulableBridged
        {
        public:
            ISchedulableBridged() = default;

            ISchedulableBridged(ISchedulableBridged&&)            = default;
            ISchedulableBridged& operator=(ISchedulableBridged&&) = default;

            virtual ~ISchedulableBridged() = default;

            virtual ETaskResumeState Resume()                  = 0;
            virtual bool             IsPaused() const noexcept = 0;
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
                        _futureState.set_value(_coro.await_resume());
                    }
                }
            }

            ETaskResumeState Resume() override
            {
                ETaskResumeState resumeState{ETaskResumeState::DONE};

                try
                {
                    resumeState = _coro.Resume();
                }
                catch (...)
                {
                    _futureState.set_exception(std::current_exception());
                    _needValueSet = false;
                }

                return resumeState;
            }

            bool IsPaused() const noexcept override { return _coro.IsPaused(); }

        private:
            bool         _needValueSet{true};
            CoroT        _coro;
            FutureStateT _futureState;
        };

    public:
        template <concepts::CoroTask CoroT, concepts::FutureState FutureStateT>
            requires (!std::is_reference_v<CoroT>) && (!std::same_as<std::decay_t<CoroT>, PackagedTask>)
        PackagedTask(CoroT&& coro, FutureStateT futureState, uint64_t pauseId)
        : id{pauseId}
        {
            using BridgeType = SchedulableBridgeImpl<CoroT, FutureStateT>;
            _pimpl = std::make_unique<BridgeType>(std::move(coro), std::move(futureState));
        }

        ETaskResumeState operator()()
        {
            return _pimpl->Resume();
        }

        bool IsPaused() const noexcept
        {
            return _pimpl->IsPaused();
        }

        const uint64_t id;

    private:
        std::unique_ptr<ISchedulableBridged> _pimpl{};
    };

} // namespace tinycoro

#endif //!__TINY_CORO_PACKAGED_CORO_HPP__