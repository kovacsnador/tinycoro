#include <gtest/gtest.h>

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

    hdl.promise().pauseHandler = std::make_shared<tinycoro::PauseHandler>([&called](){ called = true; });

    auto pauseResumerCallback = tinycoro::context::PauseTask(hdl);

    EXPECT_TRUE(hdl.promise().pauseHandler->IsPaused());
    
    std::invoke(pauseResumerCallback);

    EXPECT_TRUE(called);
}