#ifndef __TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__
#define __TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__

#include <gmock/gmock.h>

#include <coroutine>
#include <memory>
#include <concepts>

#include <tinycoro/Promise.hpp>
#include <tinycoro/PauseHandler.hpp>

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

    template<typename T = void, typename InitialCancellablePolicyT = tinycoro::noninitial_cancellable_t>
    auto MakeCoroutineHdl(std::regular_invocable auto pauseResumerCallback)
    {
        tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>> hdl;
        hdl.promise().pauseHandler.emplace(pauseResumerCallback, InitialCancellablePolicyT::value);
        return hdl;
    }

    template<typename T = void, typename InitialCancellablePolicyT = tinycoro::noninitial_cancellable_t>
    auto MakeCoroutineHdl()
    {
        tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>> hdl;
        hdl.promise().pauseHandler.emplace([]{}, InitialCancellablePolicyT::value);
        return hdl;
    }

}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__