#ifndef __TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__
#define __TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__

#include <gmock/gmock.h>

#include <tinycoro/PauseHandler.hpp>

namespace tinycoro { namespace test {

    template <typename T>
    struct TaskMockImpl
    {
        MOCK_METHOD(tinycoro::ETaskResumeState, Resume, ());
        MOCK_METHOD(T, await_resume, ());
        MOCK_METHOD(bool, IsPaused, (), (const noexcept));
        MOCK_METHOD(void, SetPauseHandler, (tinycoro::PauseHandlerCallbackT));

        tinycoro::PauseHandlerCallbackT pauseCallback;
    };

    template <typename T>
    struct PromiseMock
    {
        using value_type = T;
    };

    template <typename T>
    struct TaskMock
    {
        using promise_type = PromiseMock<T>;

        TaskMock()
        : mock{std::make_shared<TaskMockImpl<T>>()}
        {
        }

        tinycoro::ETaskResumeState Resume() { return mock->Resume(); }

        T await_resume() { return mock->await_resume(); }

        bool IsPaused() const noexcept { return mock->IsPaused(); }

        void SetPauseHandler(tinycoro::PauseHandlerCallbackT func)
        {
            mock->SetPauseHandler(func);
            mock->pauseCallback = func;
        }

        std::shared_ptr<TaskMockImpl<T>> mock;
    };
}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__