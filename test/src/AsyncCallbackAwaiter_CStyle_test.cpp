#include <gtest/gtest.h>

#include <tinycoro/AsyncCallbackAwaiter.hpp>
#include <tinycoro/Promise.hpp>

#include "mock/CoroutineHandleMock.h"

#include <future>
#include <chrono>

using namespace std::chrono_literals;

template <typename T>
struct AsyncCallbackAwaiter_CStyleTest : public testing::Test
{
    using value_type = T;

    AsyncCallbackAwaiter_CStyleTest()
    {
        hdl.promise().pauseHandler.emplace([this]() { pauseHandlerCalled = true; });
    }

    bool pauseHandlerCalled{false};

    tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<T>> hdl;
};

using AsyncCallbackAwaiter_CStyleTest_Int32 = AsyncCallbackAwaiter_CStyleTest<int32_t>;
using AsyncCallbackAwaiter_CStyleTest_Void  = AsyncCallbackAwaiter_CStyleTest<void>;

void AsyncCallbackAwaiterTest1(const bool& pauseHandlerCalled, auto hdl)
{
    struct UserData
    {
        bool data{false};
    };

    auto cb = []([[maybe_unused]] bool b, void* userData, int32_t i) {
        EXPECT_EQ(i, 42);

        auto data = static_cast<UserData*>(userData);

        EXPECT_EQ(data->data, false);
        data->data = true;
    };
    auto asyncCallback = [](auto cb, auto userData) { cb(true, userData, 42); };

    UserData uData;

    tinycoro::AsyncCallbackAwaiter_CStyle awaiter{asyncCallback, cb, tinycoro::IndexedUserData<1>(&uData)};

    EXPECT_FALSE(awaiter.await_ready());
    awaiter.await_suspend(hdl);

    if constexpr (!std::same_as<decltype(awaiter.await_resume()), void>)
    {
        EXPECT_TRUE(false) << "await_resume return not void!";
    }

    EXPECT_EQ(uData.data, true);
    EXPECT_TRUE(pauseHandlerCalled);
}

void AsyncCallbackAwaiterTest2(const bool& pauseHandlerCalled, auto hdl)
{
    int32_t uData{0};

    auto cb = [](void* userData, int32_t i) {
        EXPECT_EQ(i, 42);

        auto data = static_cast<int32_t*>(userData);

        EXPECT_EQ(*data, 0);
        *data = 42;
    };
    auto asyncCallback = [](auto cb, auto userData) {
        cb(userData, 42);
        return 44;
    };

    tinycoro::AsyncCallbackAwaiter_CStyle awaiter{asyncCallback, cb, tinycoro::IndexedUserData<0>(&uData)};

    EXPECT_FALSE(awaiter.await_ready());
    awaiter.await_suspend(hdl);

    if constexpr (!std::same_as<decltype(awaiter.await_resume()), int32_t&&>)
    {
        EXPECT_TRUE(false) << "await_resume return not int32_t&&!";
    }

    EXPECT_EQ(awaiter.await_resume(), 44);
    EXPECT_EQ(uData, 42);
    EXPECT_TRUE(pauseHandlerCalled);
}

void AsyncCallbackAwaiterTest3(const bool& pauseHandlerCalled, auto hdl)
{
    struct UserData
    {
        bool data{false};
    };

    auto cb = [](void* userData, int32_t i) {
        EXPECT_EQ(i, 42);

        auto data = static_cast<UserData*>(userData);

        EXPECT_EQ(data->data, false);
        data->data = true;

        return 44;
    };


    auto asyncCallback = [](auto cb, auto userData) { 
         return std::async(std::launch::async, [cb, userData] {
            std::this_thread::sleep_for(100ms);
            return cb(userData, 42);
        }); };

    UserData uData;

    tinycoro::AsyncCallbackAwaiter_CStyle awaiter{asyncCallback, cb, tinycoro::IndexedUserData<0>(&uData)};

    EXPECT_FALSE(awaiter.await_ready());
    awaiter.await_suspend(hdl);

    auto future = awaiter.await_resume();

    EXPECT_EQ(future.get(), 44);
    EXPECT_EQ(uData.data, true);
    EXPECT_TRUE(pauseHandlerCalled);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Int32, AsyncCallbackAwaiterTest)
{
    AsyncCallbackAwaiterTest1(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Void, AsyncCallbackAwaiterTest)
{
    AsyncCallbackAwaiterTest1(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Int32, AsyncCallbackAwaiterTest_ReturnValue)
{
    AsyncCallbackAwaiterTest2(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Void, AsyncCallbackAwaiterTest_ReturnValue)
{
    AsyncCallbackAwaiterTest2(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Int32, AsyncCallbackAwaiterTest_AsyncReturnValue)
{
    AsyncCallbackAwaiterTest3(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Void, AsyncCallbackAwaiterTest_AsyncReturnValue)
{
    AsyncCallbackAwaiterTest3(pauseHandlerCalled, hdl);
}