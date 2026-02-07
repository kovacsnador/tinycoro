#ifndef __TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__
#define __TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__

#include <gmock/gmock.h>

#include <tinycoro/Common.hpp>

#include "CoroutineHandleMock.h"

namespace tinycoro { namespace test {

    template <typename T>
    struct TaskMockImpl
    {
        MOCK_METHOD(void, Resume, ());
        MOCK_METHOD(tinycoro::detail::ETaskResumeState, ResumeState, ());
        MOCK_METHOD(T, await_resume, ());
        MOCK_METHOD(bool, IsPaused, (), (const noexcept));
        MOCK_METHOD(bool, IsDone, (), (const noexcept));
        MOCK_METHOD(void, SetResumeCallback, (tinycoro::ResumeCallback_t));
        MOCK_METHOD(void*, Address, (), (const noexcept));
        MOCK_METHOD(CoroutineHandleMock<tinycoro::detail::Promise<void>>, Release, (), (noexcept));

        tinycoro::ResumeCallback_t pauseCallback;
    };

    template <typename T>
    struct PromiseMock
    {
        using value_type = T;
    };

    template <typename T, typename CancellableT = tinycoro::initial_cancellable_t>
    struct TaskMock
    {
        using promise_type = PromiseMock<T>;
        using value_type = T;
        using initial_cancellable_policy_t = CancellableT;

        TaskMock()
        : mock{std::make_shared<TaskMockImpl<T>>()}
        {
            ON_CALL(*mock, IsDone).WillByDefault(::testing::Return(true));
        }

        void Resume() { mock->Resume(); }

        auto ResumeState() { return mock->ResumeState(); }

        T await_resume() { return mock->await_resume(); }

        bool IsDone() { return mock->IsDone(); }

        void SetResumeCallback(tinycoro::ResumeCallback_t func)
        {
            mock->SetResumeCallback(func);
            mock->pauseCallback = func;
        }

        void* Address() const noexcept
        {
            return mock->Address();
        }

        auto Release() noexcept
        {
            return mock->Release();
        }

        std::shared_ptr<TaskMockImpl<T>> mock;
    };
}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__