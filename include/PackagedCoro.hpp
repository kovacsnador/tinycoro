#ifndef __TINY_CORO_PACKAGED_CORO_HPP__
#define __TINY_CORO_PACKAGED_CORO_HPP__

#include <concepts>
#include <variant>

#include "InPlaceStorage.hpp"
#include "Common.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
		concept FutureState = (requires(T f) { { f.set_value() }; } || requires(T f) { { f.set_value(f.get_future().get()) }; } && requires(T f) { f.set_exception(std::exception_ptr{}); });

        template <typename T>
        concept CoroTask = requires (T c) {
            { c.resume() } -> std::same_as<ECoroResumeState>;
            { c.await_resume() };
            {
                c.pause([] {})
            };
        };

        template <typename T>
        concept RValueReference = std::is_rvalue_reference_v<T>;
    } // namespace concepts

    template <std::unsigned_integral auto BUFFER_SIZE = 48u>
    struct PackagedCoro
    {
        using PauseCallbackType = std::function<void()>;

    private:
        class ISchedulableBridged
        {
        public:
            ISchedulableBridged() = default;

            ISchedulableBridged(ISchedulableBridged&&)            = default;
            ISchedulableBridged& operator=(ISchedulableBridged&&) = default;

            virtual ~ISchedulableBridged()                    = default;
            virtual ECoroResumeState resume()                 = 0;
            virtual void             pause(PauseCallbackType) = 0;
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

            SchedulableBridgeImpl(SchedulableBridgeImpl&&)            = default;
            SchedulableBridgeImpl& operator=(SchedulableBridgeImpl&&) = default;

            ~SchedulableBridgeImpl()
            {
                if (_exceptionSet == false)
                {
                    if constexpr (requires {
                                      { _coro.await_resume() } -> concepts::RValueReference;
                                  })
                    {
                        _futureState.set_value(_coro.await_resume());
                    }
                    else
                    {
                        _futureState.set_value();
                    }
                }
            }

            ECoroResumeState resume() override
            {
                ECoroResumeState resumeState{ECoroResumeState::DONE};

                try
                {
                    resumeState = _coro.resume();
                }
                catch (...)
                {
                    _futureState.set_exception(std::current_exception());
                    _exceptionSet = true;
                }

                return resumeState;
            }

            void pause(PauseCallbackType func) override { _coro.pause(std::move(func)); }

        private:
            bool         _exceptionSet{false};
            CoroT        _coro;
            FutureStateT _futureState;
        };

        using StaticStorageType  = InPlaceStorage<ISchedulableBridged, BUFFER_SIZE>;
        using DynamicStorageType = std::unique_ptr<ISchedulableBridged>;

    public:
        template <concepts::CoroTask CoroT, concepts::FutureState FutureStateT>
            requires (!std::is_reference_v<CoroT>) && (!std::same_as<std::decay_t<CoroT>, PackagedCoro>)
        PackagedCoro(CoroT&& coro, FutureStateT futureState)
        {
            using BridgeType = SchedulableBridgeImpl<CoroT, FutureStateT>;

            if constexpr (sizeof(BridgeType) <= BUFFER_SIZE)
            {
                _bridge = InPlaceStorage<ISchedulableBridged, BUFFER_SIZE>{std::type_identity<BridgeType>{}, std::move(coro), std::move(futureState)};
            }
            else
            {
                _bridge = std::make_unique<BridgeType>(std::move(coro), std::move(futureState));
            }
        }

        ECoroResumeState operator()()
        {
            return std::visit([](auto& bridge) { return bridge->resume(); }, _bridge);
        }

        void Pause(std::invocable auto pauseCallback)
        {
            std::visit([&pauseCallback](auto& bridge) { bridge->pause(std::move(pauseCallback)); }, _bridge);
        }

    private:
        std::variant<StaticStorageType, DynamicStorageType> _bridge;
    };

} // namespace tinycoro

#endif //!__TINY_CORO_PACKAGED_CORO_HPP__