#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <type_traits>

#include <tinycoro/Task.hpp>

#include "mock/CoroutineHandleMock.h"

TEST(TaskTest, TaskTest_void)
{
    auto task = []()->tinycoro::Task<void> { co_return; }();

    EXPECT_EQ(task.await_ready(), false);
    EXPECT_TRUE((std::same_as<void, decltype(task.await_resume())>));

    task.Resume();

    EXPECT_EQ(task.ResumeState(), tinycoro::ETaskResumeState::DONE);
    auto pauseResumeCallback = []{};
    task.SetPauseHandler(pauseResumeCallback);

    EXPECT_NO_THROW(task.SetStopSource(std::stop_source{}));
}

TEST(TaskTest, TaskTest_defaultReturnValue_void)
{
    EXPECT_TRUE((std::same_as<tinycoro::Task<>::value_type, void>));
}

TEST(TaskTest, TaskTest_int)
{
    auto task = []()->tinycoro::Task<int32_t> { co_return 42; }();

    EXPECT_EQ(task.await_ready(), false);
    EXPECT_TRUE((std::same_as<int32_t, decltype(task.await_resume())>));

    task.Resume();

    EXPECT_EQ(task.ResumeState(), tinycoro::ETaskResumeState::DONE);

    auto pauseResumeCallback = []{};
    task.SetPauseHandler(pauseResumeCallback);

    EXPECT_NO_THROW(task.SetStopSource(std::stop_source{}));
}

struct PauseHandlerMock
{
    PauseHandlerMock(auto cb, bool)
    : pauseResume{cb}
    {
    }

    [[nodiscard]] bool IsPaused() const noexcept { return pause.load(); }

    void ResetCallback(auto) noexcept { }

    tinycoro::PauseHandlerCallbackT pauseResume;
    std::atomic<bool>               pause{true};
};

struct PromiseMock
{
    using value_type = void;

    auto initial_suspend() { return std::suspend_always{}; }

    auto final_suspend() noexcept { return std::suspend_always{}; }

    void return_void() { }

    void unhandled_exception() { std::rethrow_exception(std::current_exception()); }

    auto get_return_object() { return std::coroutine_handle<PromiseMock>::from_promise(*this); }

    template<typename... Args>
    auto MakePauseHandler(Args&&... args)
    {
        pauseHandler = std::make_shared<PauseHandlerMock>(std::forward<Args>(args)...);
        return pauseHandler.get();
    }

    void SetStopSource(auto source)
    {
        stopSource = source;
    }

    std::shared_ptr<PauseHandlerMock> pauseHandler;
    std::stop_source stopSource{};
};

struct CoroResumerMock
{
    void Resume([[maybe_unused]] auto hdl)
    {
    }

    auto ResumeState([[maybe_unused]] auto hdl)
    {
        return tinycoro::ETaskResumeState::PAUSED;
    }
};

template<typename ReturnValueT, typename BaseT>
class PopAwaiterMock : tinycoro::detail::SingleLinkable<PopAwaiterMock<ReturnValueT, BaseT>>
{
public:
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(std::coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};

TEST(CoroTaskTest, CoroTaskTest)
{
    auto task = []()->tinycoro::detail::CoroTask<void, tinycoro::default_initial_cancellable_policy, PromiseMock, PopAwaiterMock, CoroResumerMock> { co_return; }();

    EXPECT_NO_THROW(task.SetStopSource(std::stop_source{}));

    EXPECT_EQ(task.await_ready(), true);
    EXPECT_TRUE((std::same_as<void, decltype(task.await_suspend(std::coroutine_handle<>{}))>));
    EXPECT_TRUE((std::same_as<void, decltype(task.await_resume())>));

    task.Resume();

    EXPECT_EQ(task.ResumeState(), tinycoro::ETaskResumeState::PAUSED);

    auto pauseResumeCallback = []{};
    task.SetPauseHandler(pauseResumeCallback);
}