#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <type_traits>

#include <tinycoro/Task.hpp>

#include "mock/CoroutineHandleMock.h"

template<typename T>
struct IsTaskView : std::false_type
{
};

template<typename PromiseT, typename AwaiterT, typename CoroResumerT, typename StopSourceT>
struct IsTaskView<tinycoro::CoroTaskView<PromiseT, AwaiterT, CoroResumerT, StopSourceT>> : std::true_type
{
};

TEST(TaskTest, TaskTest_void)
{
    auto task = []()->tinycoro::Task<void> { co_return; }();

    EXPECT_EQ(task.await_ready(), false);
    EXPECT_TRUE((std::same_as<void, decltype(task.await_resume())>));
    EXPECT_FALSE(task.IsPaused());
    EXPECT_EQ(task.Resume(), tinycoro::ETaskResumeState::DONE);
    EXPECT_TRUE(IsTaskView<decltype(task.TaskView())>::value);

    auto pauseResumeCallback = []{};
    task.SetPauseHandler(pauseResumeCallback);
    EXPECT_FALSE(task.IsPaused());

    EXPECT_NO_THROW(task.SetStopSource(std::stop_source{}));
}

TEST(TaskTest, TaskTest_int)
{
    auto task = []()->tinycoro::Task<int32_t> { co_return 42; }();

    EXPECT_EQ(task.await_ready(), false);
    EXPECT_TRUE((std::same_as<int32_t&&, decltype(task.await_resume())>));
    EXPECT_FALSE(task.IsPaused());
    EXPECT_EQ(task.Resume(), tinycoro::ETaskResumeState::DONE);
    EXPECT_TRUE(IsTaskView<decltype(task.TaskView())>::value);

    auto pauseResumeCallback = []{};
    task.SetPauseHandler(pauseResumeCallback);
    EXPECT_FALSE(task.IsPaused());

    EXPECT_NO_THROW(task.SetStopSource(std::stop_source{}));
}

struct PauseHandlerMock
{
    PauseHandlerMock(auto cb)
    : pauseResume{cb}
    {
    }

    [[nodiscard]] bool IsPaused() const noexcept { return pause.load(); }

    tinycoro::PauseHandlerCallbackT pauseResume;
    std::atomic<bool>               pause{true};
};

struct PromiseMock
{
    auto initial_suspend() { return std::suspend_always{}; }

    auto final_suspend() noexcept { return std::suspend_always{}; }

    void return_void() { }

    void unhandled_exception() { std::rethrow_exception(std::current_exception()); }

    auto get_return_object() { return std::coroutine_handle<PromiseMock>::from_promise(*this); }

    std::shared_ptr<PauseHandlerMock> pauseHandler;
    std::stop_source stopSource{};
};

struct CoroResumerMock
{
    tinycoro::ETaskResumeState operator()([[maybe_unused]] auto hdl, [[maybe_unused]] const auto& stopSource)
    {
        return tinycoro::ETaskResumeState::PAUSED;
    }
};

template<typename ReturnValueT, typename BaseT>
class AwaiterMock
{
public:
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};

TEST(CoroTaskTest, CoroTaskTest)
{
    auto task = []()->tinycoro::CoroTask<void, PromiseMock, AwaiterMock, CoroResumerMock> { co_return; }();

    EXPECT_NO_THROW(task.SetStopSource(std::stop_source{}));

    EXPECT_EQ(task.await_ready(), true);
    EXPECT_TRUE((std::same_as<void, decltype(task.await_suspend(std::coroutine_handle<>{}))>));
    EXPECT_TRUE((std::same_as<void, decltype(task.await_resume())>));
    
    EXPECT_EQ(task.Resume(), tinycoro::ETaskResumeState::PAUSED);

    auto pauseResumeCallback = []{};
    task.SetPauseHandler(pauseResumeCallback);
    
    EXPECT_TRUE(task.IsPaused());
    EXPECT_TRUE(IsTaskView<decltype(task.TaskView())>::value);
}