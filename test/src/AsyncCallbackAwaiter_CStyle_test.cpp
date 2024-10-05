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
        hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([this]() { pauseHandlerCalled = true; });
    }

    bool pauseHandlerCalled{false};

    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<T>> hdl;
};

using AsyncCallbackAwaiter_CStyleTest_Int32 = AsyncCallbackAwaiter_CStyleTest<int32_t>;
using AsyncCallbackAwaiter_CStyleTest_Void  = AsyncCallbackAwaiter_CStyleTest<void>;

void AsyncCallbackAwaiterTest1(const bool& pauseHandlerCalled, auto hdl)
{
    struct UserData
    {
        bool data{false};
    };

    auto cb = []([[maybe_unused]] bool b, tinycoro::UserData userData, int32_t i) {
        EXPECT_EQ(i, 42);

        auto data = static_cast<UserData*>(userData.value);

        EXPECT_EQ(data->data, false);
        data->data = true;
    };
    auto asyncCallback = [](std::function<void(bool, void*, int32_t)> cb, void* userData) { cb(true, userData, 42); };

    UserData uData;

    auto awaiter = tinycoro::MakeAsyncCallbackAwaiter_CStyle(asyncCallback, tinycoro::UserCallback{cb}, tinycoro::UserData{&uData});

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

    auto cb = [](tinycoro::UserData userData, int32_t i) {
        EXPECT_EQ(i, 42);

        auto& data = userData.Get<int32_t>(); //static_cast<int32_t*>(userData);

        EXPECT_EQ(data, 0);
        data = 42;
    };

    auto asyncCallback = [](std::function<void(void*, int32_t)> cb, void* userData) {
        cb(userData, 42);
        return 44;
    };

    auto awaiter = tinycoro::MakeAsyncCallbackAwaiter_CStyle(asyncCallback, tinycoro::UserCallback{cb}, tinycoro::UserData{&uData});

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

    auto cb = [](tinycoro::UserData userData, int32_t i) {
        EXPECT_EQ(i, 42);

        auto& data = userData.Get<UserData>();

        EXPECT_EQ(data.data, false);
        data.data = true;

        return 44;
    };


    auto asyncCallback = [](std::function<void(void*, int32_t)> cb, void* userData) { 
         return std::async(std::launch::async, [cb, userData] {
            std::this_thread::sleep_for(100ms);
            return cb(userData, 42);
        }); };

    UserData uData;

    auto awaiter = tinycoro::MakeAsyncCallbackAwaiter_CStyle(asyncCallback, tinycoro::UserCallback{cb}, tinycoro::UserData{&uData});

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

/*TEST_F(AsyncCallbackAwaiter_CStyleTest_Int32, AsyncCallbackAwaiterTest_AsyncReturnValue)
{
    AsyncCallbackAwaiterTest3(pauseHandlerCalled, hdl);
}

TEST_F(AsyncCallbackAwaiter_CStyleTest_Void, AsyncCallbackAwaiterTest_AsyncReturnValue)
{
    AsyncCallbackAwaiterTest3(pauseHandlerCalled, hdl);
}*/