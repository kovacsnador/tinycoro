#ifndef TINY_CORO_PACKAGED_TASK_HPP
#define TINY_CORO_PACKAGED_TASK_HPP

#include <concepts>
#include <variant>
#include <atomic>
#include <memory>

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

        template <typename AllocatorT>
        class ISchedulableBridged
        {
        public:
            ISchedulableBridged(AllocatorT& alloc, size_t size)
            : allocator{alloc}
            , sizeInByte{size}
            {
            }

            // disable copy and move
            ISchedulableBridged(ISchedulableBridged&&) = delete;

            virtual ~ISchedulableBridged() = default;

            virtual ETaskResumeState Resume() = 0;

            virtual void SetPauseHandler(tinycoro::PauseHandlerCallbackT) = 0;

            // need for double linkage
            ISchedulableBridged<AllocatorT>* prev{nullptr};
            ISchedulableBridged<AllocatorT>* next{nullptr};

            std::atomic<EPauseState> pauseState{EPauseState::IDLE};

            // custom allocator, getting directly
            // from the scheduler
            AllocatorT& allocator;

            // contains the size of the derived object
            const size_t sizeInByte;
        };

        template <concepts::IsCorouitneTask CoroT, concepts::FutureState FutureStateT, typename AllocatorT>
        class SchedulableBridgeImpl : public ISchedulableBridged<AllocatorT>
        {
        public:
            SchedulableBridgeImpl(CoroT&& coro, FutureStateT&& futureState, AllocatorT& alloc)
            : ISchedulableBridged<AllocatorT>{alloc, sizeof(*this)}
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
                    if (_coro.IsDone())
                    {
                        // are we on a last suspend point?
                        // That means we had no cancellation before
                        if constexpr (requires { { _coro.await_resume() } -> std::same_as<void>; })
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
                        _futureState.set_value(std::nullopt);
                    }
                }
            }

            [[nodiscard]] inline ETaskResumeState Resume() override
            {
                if (_exception == nullptr)
                {
                    // no exception happend so far
                    try
                    {
                        // reset the pause state by every resume.
                        this->pauseState.store(EPauseState::IDLE, std::memory_order_relaxed);

                        // resume the coroutine
                        _coro.Resume();

                        // return the coroutine current state
                        return _coro.ResumeState();
                    }
                    catch (...)
                    {
                        // save the exception for later
                        _exception = std::current_exception();
                    }
                }

                // if there was an exception the task is done
                return ETaskResumeState::DONE;
            }

            void SetPauseHandler(tinycoro::PauseHandlerCallbackT cb) override { _coro.SetPauseHandler(std::move(cb)); }

        private:
            CoroT              _coro;
            FutureStateT       _futureState;
            std::exception_ptr _exception{};
        };

        struct AllocatorDeleter
        {
            template <typename T>
            constexpr inline void operator()(T* ptr) noexcept
            {
                auto&      alloc = ptr->allocator;
                const auto size  = ptr->sizeInByte;

                // destroy the real object
                std::destroy_at(ptr);

                // deallocate the memory
                alloc.deallocate_bytes(ptr, size, alignof(T));
            }
        };

        template <concepts::IsAllocator AllocatorT>
        using SchedulableTask = std::unique_ptr<ISchedulableBridged<AllocatorT>, AllocatorDeleter>;

        template <concepts::IsCorouitneTask CoroT, concepts::FutureState FutureStateT, typename AllocatorT>
            requires (!std::is_reference_v<CoroT>)
        SchedulableTask<AllocatorT> MakeSchedulableTask(CoroT&& coro, FutureStateT futureState, AllocatorT& allocator)
        {
            using BridgeType = SchedulableBridgeImpl<CoroT, FutureStateT, AllocatorT>;
            auto ptr         = allocator.template new_object<BridgeType>(std::move(coro), std::move(futureState), allocator);
            return SchedulableTask<AllocatorT>{ptr};
        }

    } // namespace detail

} // namespace tinycoro

#endif // TINY_CORO_PACKAGED_TASK_HPP