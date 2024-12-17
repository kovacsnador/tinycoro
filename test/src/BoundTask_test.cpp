#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>

#include <tinycoro/tinycoro_all.h>

TEST(BoundTaskTest, BoundTaskTest_make)
{
    auto task = []() -> tinycoro::Task<std::string> { co_return "42"; };

    auto taskWrapper = tinycoro::MakeBound(task);

    EXPECT_TRUE((std::same_as<tinycoro::BoundTask<decltype(task), decltype(task())>, decltype(taskWrapper)>));
}

struct TaskWrapperMockImpl
{
    struct PauseHandlerMock
    {
    };
    struct TaskViewMock
    {
    };

    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(tinycoro::ETaskResumeState, ResumeState, ());
    MOCK_METHOD(PauseHandlerMock, SetPauseHandler, (std::function<void()>));
    MOCK_METHOD(bool, IsPaused, (), (const, noexcept));
    MOCK_METHOD(PauseHandlerMock, GetPauseHandler, (), (noexcept));
    MOCK_METHOD(TaskViewMock, TaskView, (), (const, noexcept));
    MOCK_METHOD(void, SetStopSource, (std::stop_source));
    MOCK_METHOD(void, SetDestroyNotifier, (std::function<void()>));
};

struct TaskWrapperMock
{
    void Resume() { impl->Resume(); }

    [[nodiscard]] auto ResumeState() { return impl->ResumeState(); }

    auto SetPauseHandler(auto pauseResume) { return impl->SetPauseHandler(std::move(pauseResume)); }

    bool IsPaused() const noexcept { return impl->IsPaused(); }

    auto GetPauseHandler() noexcept { return impl->GetPauseHandler(); }

    [[nodiscard]] auto TaskView() const noexcept { return impl->TaskView(); }

    template <typename T>
    void SetStopSource(T&& arg)
    {
        impl->SetStopSource(std::forward<T>(arg));
    }

    template <typename T>
    void SetDestroyNotifier(T&& cb)
    {
        impl->SetDestroyNotifier(std::forward<T>(cb));
    }

    std::shared_ptr<TaskWrapperMockImpl> impl = std::make_shared<TaskWrapperMockImpl>();
};

TEST(BoundTaskTest, BoundTaskTest_Resume)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, Resume).Times(1);

    tinycoro::BoundTask taskWrapper{ [] {}, mock};
    taskWrapper.Resume();
}

TEST(BoundTaskTest, BoundTaskTest_ResumeState)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, ResumeState).Times(1).WillOnce(testing::Return(tinycoro::ETaskResumeState::SUSPENDED));

    tinycoro::BoundTask taskWrapper{ [] {}, mock};
    auto state = taskWrapper.ResumeState();

    EXPECT_EQ(state, tinycoro::ETaskResumeState::SUSPENDED);
}

TEST(BoundTaskTest, BoundTaskTest_GetPauseHandler)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, GetPauseHandler).Times(1);

    tinycoro::BoundTask taskWrapper{[] {}, mock};
    auto pauseHandler = taskWrapper.GetPauseHandler();
    EXPECT_TRUE((std::same_as<decltype(pauseHandler), TaskWrapperMockImpl::PauseHandlerMock>));
}

TEST(BoundTaskTest, BoundTaskTest_SetPauseHandler)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, SetPauseHandler).Times(1);

    tinycoro::BoundTask taskWrapper{[] {}, mock};

    auto pauseHandler = taskWrapper.SetPauseHandler(std::function<void()>{});
    EXPECT_TRUE((std::same_as<decltype(pauseHandler), TaskWrapperMockImpl::PauseHandlerMock>));
}

TEST(BoundTaskTest, BoundTaskTest_IsPaused)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, IsPaused)
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true));

    tinycoro::BoundTask taskWrapper{[] {}, mock};

    EXPECT_FALSE(taskWrapper.IsPaused());
    EXPECT_TRUE(taskWrapper.IsPaused());
}

TEST(BoundTaskTest, BoundTaskTest_TaskView)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, TaskView).Times(1);

    tinycoro::BoundTask taskWrapper{[] {}, mock};
    auto taskView = taskWrapper.TaskView();
    EXPECT_TRUE((std::same_as<decltype(taskView), TaskWrapperMockImpl::TaskViewMock>));
}

TEST(BoundTaskTest, BoundTaskTest_SetStopSource)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, SetStopSource).Times(1);

    tinycoro::BoundTask taskWrapper{[] {}, mock};
    taskWrapper.SetStopSource(std::stop_source{});
}

TEST(BoundTaskTest, BoundTaskTest_SetDestroyNotifier)
{
    TaskWrapperMock mock;

    EXPECT_CALL(*mock.impl, SetDestroyNotifier).Times(1);

    tinycoro::BoundTask taskWrapper{[] {}, mock};
    taskWrapper.SetDestroyNotifier([]{});
}