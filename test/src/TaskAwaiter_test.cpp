#include <gtest/gtest.h>

#include <coroutine>

#include <tinycoro/TaskAwaiter.hpp>

#include "mock/CoroutineHandleMock.h"

struct PauseHandlerMock
{
    static inline size_t count{0};

    PauseHandlerMock()
    : val{count++}
    {
    }

    size_t val;
};

struct StopSourceMock
{
    static inline size_t count{0};

    StopSourceMock()
    : val{count++}
    {
    }

    size_t val;
};

template<typename ValueT>
struct HandleMock
{
    HandleMock() = default;

    template<typename T>
    HandleMock(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        value = hdl.promise().value;
        stopSource = hdl.promise().stopSource;
        pauseHandler = hdl.promise().pauseHandler;
    }

    template<typename T>
    void operator=(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        value = hdl.promise().value;
        stopSource = hdl.promise().stopSource;
        pauseHandler = hdl.promise().pauseHandler;
    }

    ValueT value;

    StopSourceMock stopSource;
    PauseHandlerMock pauseHandler;
};

template<>
struct HandleMock<void>
{
    HandleMock() = default;

    template<typename T>
    HandleMock(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        stopSource = hdl.promise().stopSource;
        pauseHandler = hdl.promise().pauseHandler;
    }

    template<typename T>
    void operator=(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        stopSource = hdl.promise().stopSource;
        pauseHandler = hdl.promise().pauseHandler;
    }

    StopSourceMock stopSource;
    PauseHandlerMock pauseHandler;
};

template<typename ValueT>
struct PromiseMock
{
    HandleMock<ValueT> child;
    HandleMock<ValueT> parent;
    StopSourceMock stopSource;
    PauseHandlerMock pauseHandler;

    ValueT&& ReturnValue() { return std::move(value); }

    ValueT value;
};

template<>
struct PromiseMock<void>
{
    HandleMock<void> child;
    HandleMock<void> parent;
    StopSourceMock stopSource;
    PauseHandlerMock pauseHandler;
};

template<typename ValueT, template<typename, typename> class AwaiterT>
struct CoroTaskMock : public AwaiterT<ValueT, CoroTaskMock<ValueT, AwaiterT>>
{
    using handle_type = tinycoro::test::CoroutineHandleMock<PromiseMock<ValueT>>;

    handle_type _hdl;
};

TEST(TaskAwaiterTest, TaskAwaiterTest_await_ready_void)
{
    CoroTaskMock<void, tinycoro::AwaiterValue> task;

    auto ready = task.await_ready();
    EXPECT_FALSE(ready);
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_ready_int)
{
    CoroTaskMock<int32_t, tinycoro::AwaiterValue> task;

    auto ready = task.await_ready();
    EXPECT_FALSE(ready);
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_resume_void)
{
    CoroTaskMock<void, tinycoro::AwaiterValue> task;

    EXPECT_TRUE(( std::same_as<decltype(task.await_resume()), void>));
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_resume_int)
{
    CoroTaskMock<int32_t, tinycoro::AwaiterValue> task;

    EXPECT_TRUE(( std::same_as<decltype(task.await_resume()), int32_t&&>));
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_suspend_int)
{
    CoroTaskMock<int32_t, tinycoro::AwaiterValue> task;

    task._hdl.promise().value = 42;

    CoroTaskMock<int32_t, tinycoro::AwaiterValue> parent;

    std::ignore = task.await_suspend(parent._hdl);

    EXPECT_EQ(parent._hdl.promise().child.value, 42);

    EXPECT_EQ(task._hdl.promise().parent.pauseHandler.val, parent._hdl.promise().pauseHandler.val);
    EXPECT_EQ(task._hdl.promise().parent.stopSource.val, parent._hdl.promise().stopSource.val);
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_suspend_void)
{
    CoroTaskMock<void, tinycoro::AwaiterValue> task;

    CoroTaskMock<void, tinycoro::AwaiterValue> parent;

    std::ignore = task.await_suspend(parent._hdl);

    EXPECT_EQ(task._hdl.promise().parent.pauseHandler.val, parent._hdl.promise().pauseHandler.val);
    EXPECT_EQ(task._hdl.promise().parent.stopSource.val, parent._hdl.promise().stopSource.val);
}