#ifndef __TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__
#define __TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__

#include <gmock/gmock.h>

#include <coroutine>
#include <memory>
#include <concepts>

#include <tinycoro/Promise.hpp>

namespace tinycoro { namespace test {

    template<typename PromiseT>
    struct CoroutineHandleMock
    {
        CoroutineHandleMock()
        : _promise{std::make_shared<PromiseT>()}
        {
        }

        PromiseT& promise() { return *_promise; }

        operator std::coroutine_handle<>() const
        {
            return std::noop_coroutine();
        }

        auto operator<=>(const CoroutineHandleMock&) const = default; 

    private:
        std::shared_ptr<PromiseT> _promise;
    };

    template<std::same_as<bool> T>
    auto ResumeCallbackTracer(T& flag) noexcept
    {
        return tinycoro::ResumeCallback_t{[](auto flag, auto, auto) { *static_cast<bool*>(flag) = true; }, std::addressof(flag)};
    }

    template<typename T>
        //requires (!std::same_as<T, bool>) && (std::integral<T> || std::unsigned_integral<T>) 
    auto ResumeCallbackTracer(T& count) noexcept
    {
        return tinycoro::ResumeCallback_t{[](auto count, auto, auto) { auto val = static_cast<T*>(count); *val = (*val) + 1; }, std::addressof(count)};
    }

    template<typename T = void, typename InitialCancellablePolicyT = tinycoro::noninitial_cancellable_t>
    auto MakeCoroutineHdl(auto pauseResumerCallback)
    {
        tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>> hdl;

        hdl.promise().CreateSharedState(InitialCancellablePolicyT::value);
        hdl.promise().SharedState()->ResetCallback(pauseResumerCallback);
        
        return hdl;
    }

    template<typename T = void, typename InitialCancellablePolicyT = tinycoro::noninitial_cancellable_t>
    auto MakeCoroutineHdl()
    {
        tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>> hdl;

        hdl.promise().CreateSharedState(InitialCancellablePolicyT::value);
        hdl.promise().SharedState()->ResetCallback(tinycoro::ResumeCallback_t{[](auto, auto, auto) {}});

        return hdl;
    }

}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__