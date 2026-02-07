// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_COMMON_HPP
#define TINY_CORO_COMMON_HPP

#include <type_traits>
#include <functional>
#include <atomic>
#include <memory>
#include <cassert>
#include <chrono>
#include <optional>
#include <coroutine>
#include <stop_token>

#include "ResumeCallback.hpp"

namespace tinycoro {

    namespace detail {

        template <typename>
        struct IsDurationT : std::false_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<const std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<volatile std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename RepT, typename PerT>
        struct IsDurationT<const volatile std::chrono::duration<RepT, PerT>> : std::true_type
        {
        };

        template <typename>
        struct IsTimePointT : std::false_type
        {
        };

        template <typename ClockT, typename DurationT>
        struct IsTimePointT<std::chrono::time_point<ClockT, DurationT>> : std::true_type
        {
        };

        template <typename ClockT, typename DurationT>
        struct IsTimePointT<const std::chrono::time_point<ClockT, DurationT>> : std::true_type
        {
        };

        template <typename ClockT, typename DurationT>
        struct IsTimePointT<volatile std::chrono::time_point<ClockT, DurationT>> : std::true_type
        {
        };

        template <typename ClockT, typename DurationT>
        struct IsTimePointT<const volatile std::chrono::time_point<ClockT, DurationT>> : std::true_type
        {
        };

        // this is the unique address type of
        // an std::coroutine_handler::address
        using address_t = void*;

        template<typename... Types>
        struct IsTupleT : std::false_type
        {
        };

        template<typename... Types>
        struct IsTupleT<std::tuple<Types...>> : std::true_type
        {
        };

        template<typename... Types>
        struct IsTupleT<const std::tuple<Types...>> : std::true_type
        {
        };

        enum class ETaskResumeState : uint8_t
        {
            SUSPENDED,
            PAUSED,
            STOPPED,
            DONE
        };

        // This need to match with the masks
        // in SharedState class.
        enum class EPauseState : uint8_t
        {
            NOTIFIED = 1, // 0000 0001
            PAUSED   = 2, // 0000 0010
            IDLE     = 3 // 0000 0011
        };

        template <typename T>
            requires std::is_enum_v<T>
        constexpr auto UTypeCast(T state) noexcept
        {
            using uType = std::underlying_type_t<T>;
            return static_cast<uType>(state);
        }

    } // namespace detail

    namespace helper
    {
        constexpr auto PowerOf2(auto val) noexcept
        {
            return (val > 0 && ((val & (val - 1)) == 0));
        }
    }

    enum class ENotifyPolicy
    {
        RESUME, // Notify for resumption
        DESTROY // notify for destruction
    };

    struct VoidType
    {
    };

    // The resume callback which resumes
    // a paused coroutine.
    // 
    // Used mainly by the awaitables.
    using ResumeCallback_t = detail::ResumeCallback<void(*)(void*, void*, ENotifyPolicy), void*, void*>;

    namespace concepts {

        template <typename T>
        concept IsSchedulable = requires (T t) {
            { t.Release() };
            { t.Address() } -> std::same_as<detail::address_t>;
            typename T::value_type;
            typename T::promise_type;
            typename T::initial_cancellable_policy_t;
        };

        template<typename T, typename... Tasks>
        concept IsScheduler = requires(T s, Tasks&&... t) {
            { s.Enqueue(std::forward<Tasks>(t)...) };
        };

        template <typename T>
        concept IsCorouitneTask = std::move_constructible<T> && requires (T c) {
            { c.Resume() } -> std::same_as<void>;
            { c.IsDone() } -> std::same_as<bool>;
            { c.await_resume() };
            { c.Release() };
            { c.ResumeState() } -> std::same_as<detail::ETaskResumeState>;
            { c.SetResumeCallback(ResumeCallback_t{}) };
            typename T::value_type;
        };

        template <typename T>
        concept Iterable = requires (T) {
            typename std::decay_t<T>::iterator;
            typename std::decay_t<T>::value_type;
        };

        template <typename T>
        concept NonIterable = !Iterable<T>;

        template <typename T>
        concept HasVirtualDestructor = std::has_virtual_destructor_v<T>;

        template <typename T>
        concept Linkable = requires (std::remove_pointer_t<T>* n) {
            { n->next } -> std::same_as<std::remove_pointer_t<T>*&>;
        };

        template <typename T>
        concept DoubleLinkable = requires (std::remove_pointer_t<T>* n) {
            { n->next } -> std::same_as<std::remove_pointer_t<T>*&>;
            { n->prev } -> std::same_as<std::remove_pointer_t<T>*&>;
        };

        template <typename... Ts>
        concept IsDuration = detail::IsDurationT<Ts...>::value;

        template <typename... Ts>
        concept IsTimePoint = detail::IsTimePointT<Ts...>::value;

        template <typename T>
        concept IsNothrowInvokeable = std::is_nothrow_invocable_v<T>;

        template <typename T>
        concept IsAwaiter = requires (T t, std::coroutine_handle<> hdl) {
            { t.await_ready() };
            { t.await_suspend(hdl) };
            { t.await_resume() };
        };

        template <typename T>
        concept IsCancellableAwait = requires (T awaiter) {
            { awaiter.Cancel() } -> std::same_as<bool>;
            { awaiter.Notify() };
        };

        template <typename T>
        concept IsResumeCallbackType = std::regular_invocable<T, ENotifyPolicy>;

        template <typename T>
		concept FutureState = ( requires(T f) { { f.set_value() }; } 
                                || requires(T f) { { f.set_value(f.get_future().get().value()) }; }) 
                            && requires(T f) { f.set_exception(std::exception_ptr{}); };

        template <typename T>
        concept IsAllocatorAdapterNoException = (noexcept(std::declval<T>().operator new(42u)) && requires (T a) {
            { a.operator new(42u) } -> std::same_as<void*>;
            { a.operator delete(nullptr, 42u) } -> std::same_as<void>;
            { a.get_return_object_on_allocation_failure() };
        });

        template <typename T>
        concept IsAllocatorAdapterException = (!noexcept(std::declval<T>().operator new(42u)) && requires (T a) {
            { a.operator new(42u) } -> std::same_as<void*>;
            { a.operator delete(nullptr, 42u) } -> std::same_as<void>;
        });

        template <typename T>
        concept IsAllocatorAdapterT = IsAllocatorAdapterNoException<T> || IsAllocatorAdapterException<T> || std::is_empty_v<T>;

        template <template <typename> class T>
        concept IsAllocatorAdapter = IsAllocatorAdapterT<T<int>>;

        template <typename T>
        concept IsStopSource = requires (std::decay_t<T> t) {
            { t.request_stop() } -> std::same_as<bool>;
            { t.get_token() };
            { t.stop_requested() } -> std::same_as<bool>;
            { t.stop_possible() } -> std::same_as<bool>;
            { T{std::nostopstate} };
        };

        template<typename T>
        concept NothrowMoveAssignable = std::is_nothrow_move_assignable_v<T>;

        template<typename... Args>
        concept IsTuple = detail::IsTupleT<Args...>::value;

    } // namespace concepts

    namespace detail {

        template <typename T>
        struct ReturnT
        {
            using value_type = T;
        };

        template <>
        struct ReturnT<void>
        {
            using value_type = VoidType; // void;
        };

        template <typename T>
        struct FutureReturnT
        {
            using value_type = std::optional<typename ReturnT<T>::value_type>;
        };

        template <typename T>
        struct TaskResultType
        {
            using value_type = std::optional<typename ReturnT<T>::value_type>;
        };

        template <typename T>
        using TaskResult_t = typename TaskResultType<T>::value_type;

        template <auto Number>
        struct IsPowerOf2
        {
            static constexpr bool value = helper::PowerOf2(Number);
        };

        template <typename ReturnT, template <typename> class FutureStateT>
        struct FutureTypeGetter
        {
            // futureReturn_t is packed in a std::optinal
            using futureReturn_t = typename detail::FutureReturnT<ReturnT>::value_type;

            // this is the future state type
            //
            // e.g. std::promise<std::optinal<int32_t>>
            using futureState_t = FutureStateT<futureReturn_t>;

            // and this is the corresponding future type
            //
            // e.g. std::future<std::optinal<int32_t>>
            using future_t = decltype(std::declval<futureState_t>().get_future());
        };

        namespace helper {

            // Only for debug purposes.
            constexpr bool Contains(concepts::Linkable auto current, concepts::Linkable auto elem) noexcept
            {
                while (current)
                {
                    if (current == elem)
                        return true;

                    current = current->next;
                }
                return false;
            }

            // simple auto reset event
            struct AutoResetEvent
            {

                // sets the event
                void Set() noexcept
                {
                    _flag.test_and_set(std::memory_order_release);
                    _flag.notify_all();
                }

                // Waits for the event and resets the flag
                // to prepare for the next Set()
                void WaitAndReset() noexcept
                {
                    _flag.wait(false);
                    _flag.clear(std::memory_order_release);
                }

                [[nodiscard]] bool IsSet() const noexcept { return _flag.test(std::memory_order::relaxed); }

            private:
                std::atomic_flag _flag{};
            };

        } // namespace helper
    } // namespace detail

    // Flags indicating whether tasks are initially cancellable
    // or non-cancellable.
    struct initial_cancellable_t : std::true_type
    {
    };
    
    struct noninitial_cancellable_t : std::false_type
    {
    };

    // Tinycoro is less conservative than usual
    // regarding initial cancellability.
    //
    // The default behavior here is that tasks are initially cancellable.
    // This means the coroutine can be cancelled before it starts execution.
    using default_initial_cancellable_policy = initial_cancellable_t;

    namespace concepts {
        template <typename T, typename... Args>
        concept IsAnyOf = (std::same_as<T, Args> || ...);

        template <typename T>
        concept IsInitialCancellablePolicy = IsAnyOf<T, initial_cancellable_t, noninitial_cancellable_t>;
    } // namespace concepts

} // namespace tinycoro

#endif // !TINY_CORO_COMMON_HPP