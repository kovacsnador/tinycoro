#ifndef TINY_CORO_PROMISE_BASE_HPP
#define TINY_CORO_PROMISE_BASE_HPP

#include <array>
#include <concepts>
#include <future>
#include <new>

#include "Common.hpp"
#include "IntrusivePtr.hpp"
#include "AnyObject.hpp"
#include "UnsafeFunction.hpp"

namespace tinycoro { namespace detail {

    template <std::unsigned_integral auto SIZE, typename AlignmentT = void*>
    class Storage
    {
        using Storage_t = std::array<std::byte, SIZE>;

        using Destructor_t = void(*)(Storage*);

    public:
        Storage() = default;

        // disallow copy and move
        Storage(Storage&&) = delete;

        ~Storage() { Destroy(); }

        template <typename T>
            requires (sizeof(T) <= SIZE) && (alignof(AlignmentT) >= alignof(T))
        bool Construct(T&& object)
        {
            if (_destructor == nullptr)
            {
                // The storage is not initialized yet
                // We initialize it
                auto ptr = GetAs<T>();
                std::construct_at(ptr, std::move(object));

                // Setting the corresponding destructor
                _destructor = [](auto storage) {
                    auto ptr = storage->template GetAs<T>();
                    std::destroy_at(ptr);
                };

                return true;
            }
            return false;
        }

        // helper function to get
        // out the object pointer
        template <typename T>
            requires (sizeof(T) <= SIZE)
        T* GetAs()
        {
            return std::launder(reinterpret_cast<T*>(_buffer.data()));
        }

        auto Data() noexcept { return _buffer.data(); }

        void Destroy() noexcept
        {
            if (_destructor)
            {
                // we holding a valid object
                // in the buffer, so we need
                // to clean up
                _destructor(this);
                _destructor = nullptr;
            }
        }

        [[nodiscard]] constexpr bool Empty() const noexcept { return _destructor == nullptr; }

    private:
        // the underlying buffer
        // which stores the real object
        alignas(AlignmentT) Storage_t _buffer;

        // The dedicated destructor
        Destructor_t _destructor{nullptr};
    };

// If you want to use your own promise/future type,
// and the default buffer size is too small,
// adjust this value accordingly.
#ifndef CUSTOM_PROMISE_BUFFER_SIZE
        static constexpr std::size_t PROMISE_BASE_BUFFER_SIZE = sizeof(std::promise<int64_t>);
#else
        static constexpr std::size_t PROMISE_BASE_BUFFER_SIZE = CUSTOM_PROMISE_BUFFER_SIZE;
#endif

    // This is the base class of the promise type.
    // It is necessary to support different return types in promise types.
    //
    // IMPORTANT: This base class must be the first base class
    // from which the derived promise type inherits.
    // The reason is due to alignment within the coroutine frame.
    // The coroutine promise is laid out in memory according to the
    // most derived type, but if we have a base class like this,
    // it is only safe to use PromiseBase if it appears first
    // in the inheritance list.
    //
    // So with this base class we can convert any derived promise
    // to std::coroutine_handle<PromiseBase>.
    // It is used inside the scheduler logic.
    template <std::unsigned_integral auto BUFFER_SIZE, concepts::IsAwaiter FinalAwaiterT, concepts::PauseHandler PauseHandlerT, typename StopSourceT>
    struct PromiseBase
    {
        static_assert(BUFFER_SIZE >= PROMISE_BASE_BUFFER_SIZE, "PromiseBase: Buffer size is too small to hold the promise object.");

        using OnFinishCallback_t = void(*)(void*, void*);

        using PromiseBase_t = PromiseBase;

        PromiseBase() = default;

        // Disallow copy and move
        PromiseBase(PromiseBase&&) = delete;

        // to support lists
        PromiseBase* next{nullptr};
        PromiseBase* prev{nullptr};

        PromiseBase* child{nullptr};
        PromiseBase* parent{nullptr};

        detail::IntrusivePtr<PauseHandlerT> pauseHandler;

        // Pause state needed by the scheduler.
        std::atomic<EPauseState> pauseState{EPauseState::IDLE};

        // at the beginning we not initialize
        // the stop source here, the initialization
        // will be delayed, until we actually need this object
        StopSourceT stopSource{std::nostopstate};

        // callback to notify others if
        // the coroutine is destroyed.
        //std::function<void()> destroyNotifier;
        detail::UnsafeFunction<void(void*)> destroyNotifier;

        // Stores the exception pointer
        // if there was an unhandled_exception.
        //
        // It is set in the SchedulableTaskT.
        std::exception_ptr exception{nullptr};

        [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

        [[nodiscard]] FinalAwaiterT final_suspend() const noexcept { return {}; }

        constexpr void unhandled_exception()
        {
            std::rethrow_exception(std::current_exception());
        }

        auto& PauseState() noexcept { return pauseHandler->pauseState; }

        template <typename PromiseT, typename OnFinishCallbackT>
        bool SavePromise(PromiseT&& promise, OnFinishCallbackT finishCb)
        {
            _onFinish = finishCb;
            return _futureStateBuffer.Construct(std::forward<PromiseT>(promise));
        }

        void SaveAnyFunction(detail::AnyObject&& anyFunc)
        {
            assert(_anyFunction == false);
            _anyFunction = std::move(anyFunc);
        }

        template <typename... Args>
        auto MakePauseHandler(Args&&... args)
        {
            pauseHandler.emplace(std::forward<Args>(args)...);
            return pauseHandler.get();
        }

        template <typename T>
            requires std::constructible_from<StopSourceT, T>
        void SetStopSource(T&& arg)
        {
            stopSource        = std::forward<T>(arg);
            //triggerStopOnExit = true;
        }

        template <std::regular_invocable T>
        void SetDestroyNotifier(T&& cb) noexcept
        {
            destroyNotifier = std::forward<T>(cb);
        }

        // Getting the corresponding stop source,
        // and make sure it is initialized
        auto& StopSource() noexcept
        {
            if (stopSource.stop_possible() == false)
            {
                // initialize stop source
                // if it has no state yet
                stopSource = {};
            }
            return stopSource;
        }

        void Finish() noexcept
        {
            // This logic was previously in the destructor of PromiseBase,
            // but that caused a problem: the typed Promise, which holds
            // the return value, gets destroyed before the base Promise.
            //
            // so exported in a separete funcion, and need to be invoked
            // before the corouitne destroy call.
            if (_onFinish)
            {
                assert(_futureStateBuffer.Empty() == false);

                // setting the promise object
                // if there is one connected
                _onFinish(this, _futureStateBuffer.Data());
            }

            if (parent == nullptr)
            {
                // The parent is nullptr,
                // that means this is a root 
                // coroutine promise.
                //
                // Only trigger stop, if this
                // is a parent coroutine
                stopSource.request_stop();
            }

            if (destroyNotifier)
            {
                // notify others that the task
                // is destroyed.
                destroyNotifier();
            }
        }

    private:
        // this is the on finish callback
        // It is only invoked, if the corouitne is done.
        //
        // The first void* parameter is the _promise object.
        // In that point only the _onFinish callback
        // knows the real type of the promise.
        //
        // This _onFinish needs to be invoked in the
        // derived promise destructor, the reason is, it uses
        // the return value of the task from the derived promise.
        OnFinishCallback_t _onFinish{nullptr};

        // buffer to store the promise object
        // NOT the coroutine promise, but the
        // promise like std::promise<>
        // or tinycoro::detail::UnsafePromise<>
        Storage<BUFFER_SIZE> _futureStateBuffer;

        // holding the underlying function
        // in case we use the bound task idiom
        detail::AnyObject _anyFunction{};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_PROMISE_BASE_HPP