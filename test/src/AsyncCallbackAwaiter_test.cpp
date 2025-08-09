#include <gtest/gtest.h>

#include <tinycoro/AsyncCallbackAwaiter.hpp>
#include <tinycoro/Promise.hpp>

#include "mock/CoroutineHandleMock.h"

#include <future>
#include <chrono>

using namespace std::chrono_literals;

template <typename T>
struct AsyncCallbackAwaiterTest : public testing::Test
{
    using value_type = T;

    AsyncCallbackAwaiterTest()
    {
        hdl.promise().pauseHandler.emplace([this]([[maybe_unused]] tinycoro::ENotifyPolicy policy) { pauseHandlerCalled = true; });
    }

    bool pauseHandlerCalled{false};

    tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>> hdl;
};

using AsyncCallbackAwaiterTest_Int32 = AsyncCallbackAwaiterTest<int32_t>;
using AsyncCallbackAwaiterTest_Void  = AsyncCallbackAwaiterTest<void>;

void AsyncCallbackAwaiterTest1(const bool& pauseHandlerCalled, auto hdl)
{
    int32_t i             = 0;
    auto    cb            = [&i] { i = 42; };
    auto    asyncCallback = [](auto cb) { cb(); };

    tinycoro::AsyncCallbackAwaiter awaiter{asyncCallback, cb};

    EXPECT_FALSE(awaiter.await_ready());
    awaiter.await_suspend(hdl);

    if constexpr (!std::same_as<decltype(awaiter.await_resume()), void>)
    {
        EXPECT_TRUE(false) << "await_resume return not void!";
    }

    EXPECT_EQ(i, 42);
    EXPECT_TRUE(pauseHandlerCalled);
}

void AsyncCallbackAwaiterTest2(const bool& pauseHandlerCalled, auto hdl)
{
    int32_t i  = 0;
    auto    cb = [&i] { i = 42; };

    auto asyncCallback = [](auto cb) {
        cb();
        return 44;
    };

    tinycoro::AsyncCallbackAwaiter awaiter{asyncCallback, cb};

    awaiter.await_suspend(hdl);
    auto returnVal = awaiter.await_resume();

    EXPECT_EQ(i, 42);
    EXPECT_EQ(returnVal, 44);
    EXPECT_TRUE(pauseHandlerCalled);
}

void AsyncCallbackAwaiterTest3(const bool& pauseHandlerCalled, auto hdl)
{
    std::atomic_int32_t i  = 0;
    auto                cb = [&i] { i = 42; };

    auto asyncCallback = [](auto cb) {
        return std::async(std::launch::async, [cb] {
            std::this_thread::sleep_for(100ms);
            cb();
            return 44;
        });
    };

    tinycoro::AsyncCallbackAwaiter awaiter{asyncCallback, cb};

    awaiter.await_suspend(hdl);
    auto future = awaiter.await_resume();

    EXPECT_EQ(future.get(), 44);
    EXPECT_EQ(i, 42);
    EXPECT_TRUE(pauseHandlerCalled);
}

TEST_F(AsyncCallbackAwaiterTest_Int32, AsyncCallbackAwaiterTest)
{
    AsyncCallbackAwaiterTest1(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiterTest_Void, AsyncCallbackAwaiterTest)
{
    AsyncCallbackAwaiterTest1(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiterTest_Int32, AsyncCallbackAwaiterTest_ReturnValue)
{
    AsyncCallbackAwaiterTest2(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiterTest_Void, AsyncCallbackAwaiterTest_ReturnValue)
{
    AsyncCallbackAwaiterTest2(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiterTest_Int32, AsyncCallbackAwaiterTest_AsyncReturnValue)
{
    AsyncCallbackAwaiterTest3(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiterTest_Void, AsyncCallbackAwaiterTest_AsyncReturnValue)
{
    AsyncCallbackAwaiterTest3(pauseHandlerCalled, hdl);
}