#include <gtest/gtest.h>

#include <coroutine>

#include <tinycoro/TaskAwaiter.hpp>

#include "mock/CoroutineHandleMock.h"

struct SharedStateMock
{
    static inline size_t count{0};

    SharedStateMock()
    : val{count++}
    {
    }

    void* conti;

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
        sharedState = hdl.promise().sharedState;
    }

    template<typename T>
    void operator=(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        value = hdl.promise().value;
        stopSource = hdl.promise().stopSource;
        sharedState = hdl.promise().sharedState;
    }

    ValueT value;

    StopSourceMock stopSource;
    SharedStateMock sharedState;
};

template<>
struct HandleMock<void>
{
    HandleMock() = default;

    template<typename T>
    HandleMock(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        stopSource = hdl.promise().stopSource;
        sharedState = hdl.promise().sharedState;
    }

    template<typename T>
    void operator=(tinycoro::test::CoroutineHandleMock<T> hdl)
    {
        stopSource = hdl.promise().stopSource;
        sharedState = hdl.promise().sharedState;
    }

    StopSourceMock stopSource;
    SharedStateMock sharedState;
};

template<typename ValueT>
struct PromiseMock
{
    PromiseMock<ValueT>* conti;
    StopSourceMock stopSource;
    SharedStateMock sharedState;

    auto& StopSource() noexcept
    {
        return stopSource;
    }

    void AssignSharedState(auto) {}

    auto SharedState() { return std::addressof(sharedState); }

    ValueT&& value() { return std::move(_value); }

    ValueT _value;
};

template<>
struct PromiseMock<void>
{
    PromiseMock<void>* conti;
    StopSourceMock stopSource;
    SharedStateMock sharedState;

    auto& StopSource() noexcept
    {
        return stopSource;
    }

    void AssignSharedState(auto) {}

    auto SharedState() { return std::addressof(sharedState); }
};

template<typename ValueT, template<typename, typename> class AwaiterT>
struct CoroTaskMock : public AwaiterT<ValueT, CoroTaskMock<ValueT, AwaiterT>>
{
    using handle_type = tinycoro::test::CoroutineHandleMock<PromiseMock<ValueT>>;

    void InitConti()
    {   
        _hdl.promise().sharedState.conti = &_hdl.promise();
    }

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

    EXPECT_TRUE(( std::same_as<decltype(task.await_resume()), int32_t>));
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_suspend_int)
{
    CoroTaskMock<int32_t, tinycoro::AwaiterValue> task;

    task._hdl.promise()._value = 42;

    CoroTaskMock<int32_t, tinycoro::AwaiterValue> parent;
    parent.InitConti();

    std::ignore = task.await_suspend(parent._hdl);

    EXPECT_EQ(task._hdl.promise().conti->sharedState.val, parent._hdl.promise().sharedState.val);
    EXPECT_EQ(task._hdl.promise().conti->stopSource.val, parent._hdl.promise().stopSource.val);
}

TEST(TaskAwaiterTest, TaskAwaiterTest_await_suspend_void)
{
    CoroTaskMock<void, tinycoro::AwaiterValue> task;

    CoroTaskMock<void, tinycoro::AwaiterValue> parent;
    parent.InitConti();

    std::ignore = task.await_suspend(parent._hdl);

    EXPECT_EQ(task._hdl.promise().conti->sharedState.val, parent._hdl.promise().sharedState.val);
    EXPECT_EQ(task._hdl.promise().conti->stopSource.val, parent._hdl.promise().stopSource.val);
}