#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <future>

#include <tinycoro/PackagedTask.hpp>

template<typename T>
struct MockTaskImpl
{
    MOCK_METHOD(tinycoro::ETaskResumeState, Resume, ());
    MOCK_METHOD(T, await_resume, ());
    MOCK_METHOD(bool, IsPaused, (), (const noexcept));
};

template<typename T>
struct MockTask
{
    MockTask()
    : mock{std::make_shared<MockTaskImpl<T>>()}
    {
    }

    tinycoro::ETaskResumeState Resume() { return mock->Resume(); }
    T await_resume() { return mock->await_resume(); }
    bool IsPaused() const noexcept { return mock->IsPaused(); }

    std::shared_ptr<MockTaskImpl<T>> mock;
};

TEST(PackagedTaskTest, PackagedTaskTest)
{
    MockTask<int32_t> task;
    std::promise<int32_t> promise;

    // Setting up expectations for the mock methods
    EXPECT_CALL(*task.mock, Resume())
        .Times(1)
        .WillOnce(::testing::Return(tinycoro::ETaskResumeState::DONE));
    
    /*EXPECT_CALL(*task.mock, await_resume())
        .Times(1)
        .WillOnce(::testing::Return(42)); // Return any value you'd expect*/
    
    EXPECT_CALL(*task.mock, IsPaused())
        .Times(1)
        .WillOnce(::testing::Return(false));

    tinycoro::PackagedTask<> packagedTask(std::move(task), std::move(promise), 0);

    EXPECT_FALSE(packagedTask.IsPaused());
    EXPECT_EQ(packagedTask(), tinycoro::ETaskResumeState::DONE);
}