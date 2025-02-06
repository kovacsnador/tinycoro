#ifndef __TINY_CORO_COMMON_HPP__
#define __TINY_CORO_COMMON_HPP__

#include <type_traits>
#include <functional>
#include <atomic>
#include <memory>
#include <cassert>
#include <chrono>

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

    namespace concepts {

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
        concept Linkable = requires (std::remove_pointer_t<T> n) {
            { n.next } -> std::same_as<std::remove_pointer_t<T>*&>;
        };

        template <typename T>
        concept DoubleLinkable = requires (std::remove_pointer_t<T> n) {
            { n.next } -> std::same_as<std::remove_pointer_t<T>*&>;
            { n.prev } -> std::same_as<std::remove_pointer_t<T>*&>;
        };

        template <typename... Ts>
        concept IsDuration = detail::IsDurationT<Ts...>::value;

        template <typename... Ts>
        concept IsTimePoint = detail::IsTimePointT<Ts...>::value;

        template<typename T>
        concept IsNothrowInvokeable = std::is_nothrow_invocable_v<T>;

    } // namespace concepts

    struct VoidType
    {
    };

    // this is the unique address type of
    // an std::coroutine_handler::address
    using address_t = void*;

    // The pause handler callback signature
    // used mainly by the scheduler
    using PauseHandlerCallbackT = std::function<void()>;

    enum class ETaskResumeState
    {
        SUSPENDED,
        PAUSED,
        STOPPED,
        DONE
    };

    namespace detail { namespace helper {

        // simple auto reset event
        struct AutoResetEvent
        {
            AutoResetEvent() = default;

            // Start with a custom flag
            AutoResetEvent(bool flag)
            : _flag{flag}
            {
            }

            // sets the event
            void Set()
            {
                _flag.store(true);
                _flag.notify_all();
            }

            // Waits for the event and resets the flag
            // to prepare for the next Set()
            bool Wait()
            {
                _flag.wait(false);

                bool expected{true};
                return _flag.compare_exchange_strong(expected, false);
            }

            [[nodiscard]] bool IsSet() const noexcept { return _flag.load(std::memory_order::relaxed); }

        private:
            std::atomic<bool> _flag{};
        };

    }} // namespace detail::helper
} // namespace tinycoro

#endif // !__TINY_CORO_COMMON_HPP__
