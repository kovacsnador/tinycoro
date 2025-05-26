#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <type_traits>

#include <tinycoro/PauseHandler.hpp>
#include <tinycoro/Task.hpp>

#include <mock/CoroutineHandleMock.h>

template<typename T>
struct Concepts_PauseHandlerCbTest : public testing::Test
{
    using value_type = T;
};

using lambdaType1 = decltype([](){});
using lambdaType2 = decltype([](std::unique_ptr<tinycoro::PauseHandler>){});

using FuncType1 = void (*)();

using PauseHandlerCbTestTypes = testing::Types<std::tuple<lambdaType1, std::true_type>,
                                                std::tuple<lambdaType2, std::false_type>,
                                                std::tuple<FuncType1, std::true_type>>;

TYPED_TEST_SUITE(Concepts_PauseHandlerCbTest, PauseHandlerCbTestTypes);

TYPED_TEST(Concepts_PauseHandlerCbTest, Concepts_PauseHandlerCbTest_test)
{
    using T = typename TestFixture::value_type;

    using firstParamT = std::decay_t<decltype(std::get<0>(std::declval<T>()))>;
    using secondParamT = std::decay_t<decltype(std::get<1>(std::declval<T>()))>;

    if constexpr (tinycoro::concepts::PauseHandlerCb<firstParamT>)
    {
        EXPECT_TRUE(secondParamT::value);
    }
    else
    {
        EXPECT_FALSE(secondParamT::value);
    }
}

template<typename T>
struct Concepts_PauseHandlerTest : public testing::Test
{
    using value_type = T;
};

using PauseHandlerTestTypes = testing::Types<std::tuple<tinycoro::PauseHandler, std::true_type>,
                                                std::tuple<std::string, std::false_type>>;

TYPED_TEST_SUITE(Concepts_PauseHandlerTest, PauseHandlerTestTypes);

TYPED_TEST(Concepts_PauseHandlerTest, Concepts_PauseHandlerTest_test)
{
    using T = typename TestFixture::value_type;

    using firstParamT = std::decay_t<decltype(std::get<0>(std::declval<T>()))>;
    using secondParamT = std::decay_t<decltype(std::get<1>(std::declval<T>()))>;

    if constexpr (tinycoro::concepts::PauseHandler<firstParamT>)
    {
        EXPECT_TRUE(secondParamT::value);
    }
    else
    {
        EXPECT_FALSE(secondParamT::value);
    }
}

TEST(PauseHandlerTest, CancellableSuspentTest_value)
{
    tinycoro::test::CoroutineHandleMock<tinycoro::Promise<int32_t>> hdl;

    bool called = false;

    hdl.promise().pauseHandler.emplace([&called](){ called = true; });

    auto pauseResumerCallback = tinycoro::context::PauseTask(hdl);

    EXPECT_TRUE(hdl.promise().pauseHandler->IsPaused());
    
    std::invoke(pauseResumerCallback);

    EXPECT_TRUE(called);
}

TEST(PauseHandlerTest, PauseHandlerTest_pause)
{
    bool called = false;

    std::function<void()> func = [&called]{ called = true; };

    tinycoro::PauseHandler pauseHandler{func};

    auto res = pauseHandler.Pause();
    EXPECT_TRUE((std::same_as<decltype(func), decltype(res)>));

    EXPECT_TRUE(pauseHandler.IsPaused());
    EXPECT_FALSE(pauseHandler.IsCancellable());

    pauseHandler.Unpause();
    EXPECT_FALSE(pauseHandler.IsPaused());

    EXPECT_FALSE(called);
}

TEST(PauseHandlerTest, PauseHandlerTest_MakeCancellable)
{
    tinycoro::PauseHandler pauseHandler{[]{}};

    EXPECT_FALSE(pauseHandler.IsCancellable());

    pauseHandler.SetCancellable(true);
    EXPECT_TRUE(pauseHandler.IsCancellable());

    pauseHandler.SetCancellable(false);
    EXPECT_FALSE(pauseHandler.IsCancellable());

    pauseHandler.SetCancellable(true);
    EXPECT_TRUE(pauseHandler.IsCancellable());
}

struct Context_PauseHandlerMock
{
    MOCK_METHOD(std::function<void()>, Pause, ());
    MOCK_METHOD(void, SetCancellable, (bool));
    MOCK_METHOD(void, Unpause, ());
};

struct Context_PromiseMock
{
    MOCK_METHOD(void, return_value, (int32_t));

    std::shared_ptr<Context_PauseHandlerMock> pauseHandler = std::make_shared<Context_PauseHandlerMock>();
};

struct Context_CoroutineHandlerMock
{   
    auto& promise() { return *p; }

    std::shared_ptr<Context_PromiseMock> p = std::make_shared<Context_PromiseMock>();
};

TEST(ContextTest, ContextTest_PauseTask)
{
    Context_CoroutineHandlerMock hdl;

    EXPECT_CALL(*hdl.p->pauseHandler, Pause).Times(1).WillOnce(testing::Return([]{}));   
    auto res = tinycoro::context::PauseTask(hdl);
}

TEST(ContextTest, ContextTest_UnpauseTask)
{
    Context_CoroutineHandlerMock hdl;

    EXPECT_CALL(*hdl.p->pauseHandler, Unpause).Times(1);   
    tinycoro::context::UnpauseTask(hdl);
}

TEST(ContextTest, ContextTest_MakeCancellable)
{
    Context_CoroutineHandlerMock hdl;

    EXPECT_CALL(*hdl.p->pauseHandler, SetCancellable(true)).Times(1);   
    tinycoro::context::MakeCancellable(hdl);
}

TEST(ContextTest, ContextTest_GetHandler)
{
    Context_CoroutineHandlerMock hdl;
  
    EXPECT_EQ(tinycoro::context::GetHandler(hdl), hdl.p->pauseHandler.get());
}