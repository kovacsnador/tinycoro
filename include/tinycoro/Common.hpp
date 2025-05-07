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

    } // namespace detail

    struct VoidType
    {
    };

    // this is the unique address type of
    // an std::coroutine_handler::address
    using address_t = void*;

    // The pause handler callback signature
    // used mainly by the scheduler
    using PauseHandlerCallbackT = std::function<void()>;

    enum class ETaskResumeState : uint8_t
    {
        SUSPENDED,
        PAUSED,
        STOPPED,
        DONE
    };

    enum class EPauseState : uint8_t
    {
        IDLE,
        PAUSED,

        NOTIFIED,
    };

    namespace concepts {

        template <typename T>
        concept IsSchedulable = requires (T t) {
            { t->Resume() } -> std::same_as<ETaskResumeState>;
            {
                t->SetPauseHandler([] { })
            } -> std::same_as<void>;
            typename T::element_type;
        };

        template <typename T>
        concept IsCorouitneTask = std::move_constructible<T> && requires (T c) {
            { c.Resume() } -> std::same_as<void>;
            { c.IsDone() } -> std::same_as<bool>;
            { c.await_resume() };
            { c.Release() };
            { c.ResumeState() } -> std::same_as<ETaskResumeState>;
            {
                c.SetPauseHandler([] { })
            };
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
        concept IsCancellableAwait = requires (T awaiter) {
            { awaiter.Cancel() } -> std::same_as<bool>;
            { awaiter.Notify() };
        };

        template <typename T>
        concept IsAllocator = requires (T alloc, int val) {
            { alloc.template new_object<int>(42) } -> std::same_as<int*>;
            { alloc.deallocate_bytes(&val, sizeof(val), alignof(decltype(val))) };
        };

        template <typename T>
        concept IsAwaiter = requires (T t) {
            { t.await_ready() };
            { t.await_suspend(std::coroutine_handle<>{}) };
            { t.await_resume() };
        };

        template <typename T>
        concept PauseHandler = std::constructible_from<T, PauseHandlerCallbackT> && requires (T t) {
            { t.IsPaused() } -> std::same_as<bool>;
        };

        template <typename T>
		concept FutureState = ( requires(T f) { { f.set_value() }; } 
                                || requires(T f) { { f.set_value(f.get_future().get().value()) }; }) 
                            && requires(T f) { f.set_exception(std::exception_ptr{}); };

    } // namespace concepts

    namespace detail {

        template <typename>
        struct FutureReturnT;

        template <typename T>
        struct FutureReturnT
        {
            using value_type = std::optional<T>;
        };

        template <>
        struct FutureReturnT<void>
        {
            using value_type = std::optional<VoidType>; // void;
        };

        template <typename T>
        struct TaskResultType
        {
            using value_type = std::optional<T>;
        };

        template <>
        struct TaskResultType<void>
        {
            using value_type = std::optional<VoidType>;
        };

        template <typename T>
        using TaskResult_t = typename TaskResultType<T>::value_type;

        template <auto Number>
        struct IsPowerOf2
        {
            static constexpr bool value = (Number > 0) && (Number & (Number - 1)) == 0;
        };

        namespace helper {

            bool Contains(concepts::Linkable auto current, concepts::Linkable auto elem)
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
                AutoResetEvent() = default;

                // Start with a custom flag
                explicit AutoResetEvent(bool flag)
                : _flag{flag}
                {
                }

                // sets the event
                void Set()
                {
                    _flag.store(true, std::memory_order_release);
                    _flag.notify_all();
                }

                // Waits for the event and resets the flag
                // to prepare for the next Set()
                bool Wait()
                {
                    _flag.wait(false);

                    bool expected{true};
                    return _flag.compare_exchange_strong(expected, false, std::memory_order_release, std::memory_order_relaxed);
                }

                [[nodiscard]] bool IsSet() const noexcept { return _flag.load(std::memory_order::relaxed); }

            private:
                std::atomic<bool> _flag{};
            };

        } // namespace helper
    } // namespace detail
} // namespace tinycoro

#endif // !TINY_CORO_COMMON_HPP