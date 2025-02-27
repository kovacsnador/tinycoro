#ifndef __TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__
#define __TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__

#include <gmock/gmock.h>

#include <tinycoro/PauseHandler.hpp>

namespace tinycoro { namespace test {

    template <typename T>
    struct TaskMockImpl
    {
        MOCK_METHOD(void, Resume, ());
        MOCK_METHOD(tinycoro::ETaskResumeState, ResumeState, ());
        MOCK_METHOD(T, await_resume, ());
        MOCK_METHOD(bool, IsPaused, (), (const noexcept));
        MOCK_METHOD(bool, IsDone, (), (const noexcept));
        MOCK_METHOD(void, SetPauseHandler, (tinycoro::PauseHandlerCallbackT));
        MOCK_METHOD(void*, Address, (), (const noexcept));

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
            ON_CALL(*mock, IsDone).WillByDefault(::testing::Return(true));
        }

        void Resume() { mock->Resume(); }

        tinycoro::ETaskResumeState ResumeState() { return mock->ResumeState(); }

        T await_resume() { return mock->await_resume(); }

        bool IsDone() { return mock->IsDone(); }

        void SetPauseHandler(tinycoro::PauseHandlerCallbackT func)
        {
            mock->SetPauseHandler(func);
            mock->pauseCallback = func;
        }

        void* Address() const noexcept
        {
            return mock->Address();
        }

        std::shared_ptr<TaskMockImpl<T>> mock;
    };
}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_MOCK_TASK_MOCK_HPP__