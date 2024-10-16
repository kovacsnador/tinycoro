#ifndef __TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__
#define __TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__

#include <gmock/gmock.h>

#include <coroutine>
#include <memory>

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

}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_SRC_MOCK_COROUTINE_HANDLE_MOCK_H__