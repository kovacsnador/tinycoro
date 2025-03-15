#ifndef __TINY_CORO_PACKAGED_TASK_HPP__
#define __TINY_CORO_PACKAGED_TASK_HPP__

#include <concepts>
#include <variant>
#include <memory_resource>
#include <memory>

#include "Common.hpp"
#include "Exception.hpp"

namespace tinycoro {

    namespace concepts {

        template <typename T>
		concept FutureState = (requires(T f) { { f.set_value() }; } || requires(T f) { { f.set_value(f.get_future().get().value()) }; }) && requires(T f) { f.set_exception(std::exception_ptr{}); };

        /*template <typename T>
        concept CoroTask = std::move_constructible<T> && requires (T c) {
            { c.Resume() } -> std::same_as<void>;
            { c.IsDone() } -> std::same_as<bool>;
            { c.await_resume() };
            { c.ResumeState() } -> std::same_as<ETaskResumeState>;
            {
                c.SetPauseHandler([] { })
            };
        };*/

    } // namespace concepts

    namespace detail {

        template <typename AllocatorT>
        class ISchedulableBridged
        {
        public:
            ISchedulableBridged(AllocatorT alloc)
            : _allocator{std::move(alloc)}
            {
            }

            // disable copy and move
            ISchedulableBridged(ISchedulableBridged&&) = delete;

            virtual ~ISchedulableBridged() = default;

            virtual void             Resume()      = 0;
            virtual ETaskResumeState ResumeState() = 0;

            virtual void SetPauseHandler(tinycoro::PauseHandlerCallbackT) = 0;

            auto GetAllocator() -> AllocatorT { return _allocator; }

            // need for double linkage
            ISchedulableBridged* prev{nullptr};
            ISchedulableBridged* next{nullptr};

        private:
            AllocatorT _allocator;
        };

        template <concepts::IsCorouitneTask CoroT, /*concepts::FutureState*/typename FutureStateT, typename AllocatorT>
        class SchedulableBridgeImpl : public ISchedulableBridged<AllocatorT>
        {
        public:
            SchedulableBridgeImpl(CoroT&& coro, FutureStateT&& futureState, AllocatorT allocator)
            : ISchedulableBridged<AllocatorT>{std::move(allocator)}
            , _coro{std::move(coro)}
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
    } // namespace detail

    // using SchedulableTask = std::unique_ptr<detail::ISchedulableBridged<std::pmr::polymorphic_allocator>>;

    template <typename T>
    struct SchedulableTaskWrapper
    {
        using element_type = T;

        SchedulableTaskWrapper(T* obj)
        : _obj{obj}
        {
        }

        SchedulableTaskWrapper(SchedulableTaskWrapper&& other)
        : _obj{std::exchange(other._obj, nullptr)}
        {
        }

        ~SchedulableTaskWrapper()
        {
            if (_obj)
            {
                // delete the object with the given allocator
                auto alloc = _obj->GetAllocator();
                alloc->delete_object(_obj);
            }
        }

        T* release() noexcept { return std::exchange(_obj, nullptr); }

        const T* operator->() const noexcept { return _obj; }

        T* operator->() noexcept { return _obj; }

        auto* get() noexcept { return _obj; }

        const auto* get() const noexcept { return _obj; }

    private:
        T* _obj{};
    };

    using SchedulableTask = SchedulableTaskWrapper<detail::ISchedulableBridged<std::shared_ptr<std::pmr::polymorphic_allocator<>>>>;

    template <concepts::IsCorouitneTask CoroT, /*concepts::FutureState*/typename FutureStateT, typename AllocatorT>
        requires (!std::is_reference_v<CoroT>)
    SchedulableTask MakeSchedulableTask(CoroT&& coro, FutureStateT futureState, std::shared_ptr<AllocatorT> allocator)
    {
        using BridgeType = detail::SchedulableBridgeImpl<CoroT, FutureStateT, std::shared_ptr<AllocatorT>>;
        // return std::make_unique<BridgeType>(std::move(coro), std::move(futureState), alloc);
        return allocator->new_object<BridgeType>(std::move(coro), std::move(futureState), allocator);
    }

} // namespace tinycoro

#endif //!__TINY_CORO_PACKAGED_TASK_HPP__