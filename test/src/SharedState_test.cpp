#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <memory>
#include <type_traits>

#include <tinycoro/SharedState.hpp>
#include <tinycoro/Task.hpp>

#include <mock/CoroutineHandleMock.h>

template<typename T>
struct Concepts_PauseHandlerCbTest : public testing::Test
{
    using value_type = T;
};

using lambdaType1 = decltype([](tinycoro::ENotifyPolicy){});
using lambdaType2 = decltype([](std::unique_ptr<tinycoro::detail::SharedState>){});

using FuncType1 = void (*)(tinycoro::ENotifyPolicy);

using PauseHandlerCbTestTypes = testing::Types<std::tuple<lambdaType1, std::true_type>,
                                                std::tuple<lambdaType2, std::false_type>,
                                                std::tuple<FuncType1, std::true_type>>;

TYPED_TEST_SUITE(Concepts_PauseHandlerCbTest, PauseHandlerCbTestTypes);

TYPED_TEST(Concepts_PauseHandlerCbTest, Concepts_PauseHandlerCbTest_test)
{
    using T = typename TestFixture::value_type;

    using firstParamT = std::decay_t<decltype(std::get<0>(std::declval<T>()))>;
    using secondParamT = std::decay_t<decltype(std::get<1>(std::declval<T>()))>;

    if constexpr (tinycoro::concepts::IsResumeCallbackType<firstParamT>)
    {
        EXPECT_TRUE(secondParamT::value);
    }
    else
    {
        EXPECT_FALSE(secondParamT::value);
    }
}

TEST(SharedStateTest, CancellableSuspentTest_value)
{
    tinycoro::test::CoroutineHandleMock<tinycoro::detail::Promise<int32_t>> hdl;

    bool called = false;

    hdl.promise().SharedState()->ResetCallback(tinycoro::test::ResumeCallbackTracer(called));

    auto pauseResumerCallback = tinycoro::context::PauseTask(hdl);

    EXPECT_TRUE(hdl.promise().SharedState()->IsPaused());
    
    std::invoke(pauseResumerCallback, tinycoro::ENotifyPolicy::RESUME);

    EXPECT_TRUE(called);
}

TEST(SharedStateTest, SharedStateTest_pause)
{
    bool called = false;

    tinycoro::detail::SharedState sharedState{tinycoro::default_initial_cancellable_policy::value};
    sharedState.ResetCallback(tinycoro::test::ResumeCallbackTracer(called));

    auto res = sharedState.Pause();
    EXPECT_TRUE((std::assignable_from<decltype(res)&, tinycoro::ResumeCallback_t>));

    EXPECT_TRUE(sharedState.IsPaused());

    // initial cancellable as default
    EXPECT_TRUE(sharedState.IsCancellable());

    sharedState.Unpause();
    EXPECT_FALSE(sharedState.IsPaused());

    EXPECT_FALSE(called);
}

TEST(SharedStateTest, SharedStateTest_MakeCancellable)
{
    tinycoro::detail::SharedState sharedState{tinycoro::default_initial_cancellable_policy::value};
    
    // initial cancellable as default
    EXPECT_TRUE(sharedState.IsCancellable());
}

TEST(SharedStateTest, SharedStateTest_MakeCancellable_noninitial_cancellable)
{
    tinycoro::detail::SharedState sharedState{tinycoro::noninitial_cancellable_t::value};
    
    // initial cancellable as default
    EXPECT_FALSE(sharedState.IsCancellable());

    sharedState.MakeCancellable();
    EXPECT_TRUE(sharedState.IsCancellable());
}

TEST(SharedStateTest, SharedStateTest_ExceptionThrowned)
{
    tinycoro::detail::SharedState sharedState{tinycoro::noninitial_cancellable_t::value};
    
    EXPECT_FALSE(sharedState.HasException());
    sharedState.ClearFlags();
    EXPECT_FALSE(sharedState.HasException());

    // set the exception
    sharedState.MarkException();

    EXPECT_FALSE(sharedState.IsCancellable());
    EXPECT_FALSE(sharedState.IsPaused());
    EXPECT_TRUE(sharedState.HasException());

    sharedState.ClearFlags();
    EXPECT_TRUE(sharedState.HasException());

    EXPECT_FALSE(sharedState.IsCancellable());
    EXPECT_FALSE(sharedState.IsPaused());
}

TEST(SharedStateTest, SharedStateTest_stop_token_user)
{
    tinycoro::detail::SharedState sharedState{tinycoro::noninitial_cancellable_t::value};

    EXPECT_FALSE(sharedState.IsStopTokenUser());
    sharedState.ClearFlags();
    EXPECT_FALSE(sharedState.IsStopTokenUser());

    // set the flag
    sharedState.MarkStopTokenUser();
    EXPECT_TRUE(sharedState.IsStopTokenUser());

    sharedState.ClearFlags();
    EXPECT_TRUE(sharedState.IsStopTokenUser());
}

TEST(SharedStateTest, SharedStateTest_presistent_flags)
{
    tinycoro::detail::SharedState sharedState{tinycoro::noninitial_cancellable_t::value};

    EXPECT_FALSE(sharedState.HasException());
    EXPECT_FALSE(sharedState.IsStopTokenUser());
    sharedState.ClearFlags();
    EXPECT_FALSE(sharedState.HasException());
    EXPECT_FALSE(sharedState.IsStopTokenUser());

    // set the flag
    sharedState.MarkStopTokenUser();
    sharedState.MarkException();

    EXPECT_TRUE(sharedState.HasException());
    EXPECT_TRUE(sharedState.IsStopTokenUser());

    sharedState.ClearFlags();
    EXPECT_TRUE(sharedState.HasException());
    EXPECT_TRUE(sharedState.IsStopTokenUser());
}

struct Context_SharedStateMock
{
    MOCK_METHOD(tinycoro::ResumeCallback_t, Pause, ());
    MOCK_METHOD(void, MakeCancellable, ());
    MOCK_METHOD(void, Unpause, ());
};

struct Context_PromiseMock
{
    MOCK_METHOD(void, return_value, (int32_t));

    MOCK_METHOD(Context_SharedStateMock*, SharedState, ());

    std::shared_ptr<Context_SharedStateMock> sharedState = std::make_shared<Context_SharedStateMock>();
};

struct Context_CoroutineHandlerMock
{   
    auto& promise() { return *p; }

    std::shared_ptr<Context_PromiseMock> p = std::make_shared<Context_PromiseMock>();
};

TEST(ContextTest, ContextTest_PauseTask)
{
    Context_CoroutineHandlerMock hdl;

    EXPECT_CALL(*hdl.p, SharedState).Times(1).WillOnce(testing::Return(hdl.p->sharedState.get()));

    EXPECT_CALL(*hdl.p->sharedState, Pause).Times(1).WillOnce(testing::Return(tinycoro::ResumeCallback_t{}));
    auto res = tinycoro::context::PauseTask(hdl);

    EXPECT_TRUE((std::same_as<decltype(res), tinycoro::ResumeCallback_t>));
}

TEST(ContextTest, ContextTest_UnpauseTask)
{
    Context_CoroutineHandlerMock hdl;

    EXPECT_CALL(*hdl.p, SharedState).Times(1).WillOnce(testing::Return(hdl.p->sharedState.get()));
    EXPECT_CALL(*hdl.p->sharedState, Unpause).Times(1);   
    tinycoro::context::UnpauseTask(hdl);
}

TEST(ContextTest, ContextTest_MakeCancellable)
{
    Context_CoroutineHandlerMock hdl;

    EXPECT_CALL(*hdl.p, SharedState).Times(1).WillOnce(testing::Return(hdl.p->sharedState.get()));
    EXPECT_CALL(*hdl.p->sharedState, MakeCancellable()).Times(1);   
    tinycoro::context::MakeCancellable(hdl);
}