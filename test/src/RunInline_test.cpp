#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/tinycoro_all.h>

struct RunInline_PauseHandlerMock
{
    RunInline_PauseHandlerMock() = default;
    RunInline_PauseHandlerMock(tinycoro::PauseHandlerCallbackT c)
    : cb{c}
    {
    }

    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(void, AtomicWait, (bool));

    tinycoro::PauseHandlerCallbackT cb;
};

template<typename T>
struct PromiseMock
{
    using value_type = T;
};

template<typename ReturnT, typename PauseHandlerT>
struct RunInline_TaskMock
{

    MOCK_METHOD(ReturnT, await_resume, ());
    MOCK_METHOD(std::shared_ptr<PauseHandlerT>, GetPauseHandler, ());
    MOCK_METHOD(std::shared_ptr<PauseHandlerT>, SetPauseHandler, (tinycoro::PauseHandlerCallbackT));
    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(tinycoro::ETaskResumeState, ResumeState, ());

    std::shared_ptr<PauseHandlerT> pauseHandlerMock;
};

template<typename ReturnT, typename PauseHandlerT>
struct RunInline_TaskMockWrapper
{
    using promise_type = PromiseMock<ReturnT>;

    RunInline_TaskMockWrapper()
    : mock{std::make_shared<RunInline_TaskMock<ReturnT, PauseHandlerT>>()}
    {
    }

    ReturnT await_resume()
    {
        return mock->await_resume();
    }

    std::shared_ptr<PauseHandlerT> GetPauseHandler()
    {
        return mock->GetPauseHandler();

    }

    std::shared_ptr<PauseHandlerT> SetPauseHandler(tinycoro::PauseHandlerCallbackT cb)
    {
        return mock->SetPauseHandler(cb);

    }

    void Resume()
    {
        mock->Resume();
    }

    tinycoro::ETaskResumeState ResumeState()
    {
        return mock->ResumeState();
    }

    std::shared_ptr<RunInline_TaskMock<ReturnT, PauseHandlerT>> mock;
};

TEST(RunInlineTest, RunInlineTest_void)
{
    RunInline_TaskMock<void, RunInline_PauseHandlerMock> mock;

    EXPECT_CALL(mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto callback) {
            mock.pauseHandlerMock = std::make_shared<RunInline_PauseHandlerMock>(callback);
            return mock.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock, Resume()).Times(1);
    EXPECT_CALL(mock, ResumeState()).WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));
    EXPECT_CALL(mock, await_resume()).Times(1);

    tinycoro::RunInline(mock);
}

TEST(RunInlineTest, RunInlineTest_int32)
{
    RunInline_TaskMock<int32_t, RunInline_PauseHandlerMock> mock;

    EXPECT_CALL(mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto callback) {
            mock.pauseHandlerMock = std::make_shared<RunInline_PauseHandlerMock>(callback);
            return mock.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock, Resume()).Times(1);
    EXPECT_CALL(mock, ResumeState()).WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));
    EXPECT_CALL(mock, await_resume()).Times(1).WillOnce(testing::Return(42));

    auto val = tinycoro::RunInline(mock);
    EXPECT_EQ(42, val);
}

TEST(RunInlineTest, RunInlineTest_pause)
{
    RunInline_TaskMock<int32_t, RunInline_PauseHandlerMock> mock;

    mock.pauseHandlerMock = std::make_shared<RunInline_PauseHandlerMock>();

    EXPECT_CALL(mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto) {
            return mock.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock, Resume()).Times(2);
    EXPECT_CALL(mock, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::PAUSED))
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

    EXPECT_CALL(*mock.pauseHandlerMock, AtomicWait(true)).Times(1);

    EXPECT_CALL(mock, await_resume()).Times(1).WillOnce(testing::Return(42));

    auto val = tinycoro::RunInline(mock);
    EXPECT_EQ(42, val);
}

TEST(RunInlineTest, RunInlineTest_multiTasks)
{
    RunInline_TaskMock<int32_t, RunInline_PauseHandlerMock> mock1;

    mock1.pauseHandlerMock = std::make_shared<RunInline_PauseHandlerMock>();

    EXPECT_CALL(mock1, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock1] (auto) {
            return mock1.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock1, Resume()).Times(1);
    EXPECT_CALL(mock1, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

    EXPECT_CALL(mock1, await_resume()).Times(1).WillOnce(testing::Return(42));

    RunInline_TaskMock<int32_t, RunInline_PauseHandlerMock> mock2;

    mock2.pauseHandlerMock = std::make_shared<RunInline_PauseHandlerMock>();

    EXPECT_CALL(mock2, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock2] (auto) {
            return mock2.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock2, Resume()).Times(2);
    EXPECT_CALL(mock2, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::PAUSED))
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::STOPPED));

    EXPECT_CALL(*mock2.pauseHandlerMock, AtomicWait(true)).Times(1);

    EXPECT_CALL(mock2, await_resume()).Times(1).WillOnce(testing::Return(43));

    auto [result1, result2] = tinycoro::RunInline(mock1, mock2);
    EXPECT_EQ(42, result1);
    EXPECT_EQ(43, result2);
}

TEST(RunInlineTest, RunInlineTest_dynamicTasks)
{
    std::vector<RunInline_TaskMockWrapper<int32_t, RunInline_PauseHandlerMock>> tasks;

    for(size_t i=0; i < 10; ++i)
    {
        tasks.emplace_back();

        tasks[i].mock->pauseHandlerMock = std::make_shared<RunInline_PauseHandlerMock>();

        EXPECT_CALL(*tasks[i].mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&tasks, index = i] (auto) {
            return tasks[index].mock->pauseHandlerMock;
        }
        ));

        EXPECT_CALL(*tasks[i].mock, Resume()).Times(1);
        EXPECT_CALL(*tasks[i].mock, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

        EXPECT_CALL(*tasks[i].mock, await_resume()).Times(1).WillOnce(testing::Return(42));
    }

    auto results = tinycoro::RunInline(tasks);
    std::ranges::for_each(results, [](const auto& v) {
        EXPECT_EQ(42, v);
    });
}

TEST(RunInlineTest, RunInline_FunctionalTest_voidTasks)
{
    size_t i = 0;

    auto consumer1 = [&]()->tinycoro::Task<void>
    {
        EXPECT_EQ(i++, 0);
        co_return; 
    };

    auto consumer2 = [&]()->tinycoro::Task<void>
    {
        EXPECT_EQ(i++, 1);
        co_return; 
    };

    auto consumer3 = [&]()->tinycoro::Task<void>
    {
        EXPECT_EQ(i++, 2);
        co_return; 
    };

    tinycoro::RunInline(consumer1(), consumer2(), consumer3());
    EXPECT_EQ(i, 3);
}

TEST(RunInlineTest, RunInline_FunctionalTest_1)
{
    tinycoro::SingleEvent<int32_t> event;

    auto consumer = [&event]()->tinycoro::Task<int32_t>
    {
        co_return co_await event; 
    };

    // set the value
    event.SetValue(42);

    auto value = tinycoro::RunInline(consumer());
    EXPECT_EQ(value, 42);
}

TEST(RunInlineTest, RunInline_FunctionalTest_2)
{
    tinycoro::SingleEvent<int32_t> event;

    auto consumer = [&event]()->tinycoro::Task<int32_t>
    {
        co_return co_await event; 
    };

    auto producer = [&event]()->tinycoro::Task<void>
    {
        co_await tinycoro::Sleep(100ms);
        event.SetValue(42);
    };

    tinycoro::Scheduler scheduler{1};

    std::ignore = scheduler.Enqueue(producer());

    auto value = tinycoro::RunInline(consumer());
    EXPECT_EQ(value, 42);
}

TEST(RunInlineTest, RunInline_FunctionalTest_3)
{
    int32_t count{};

    auto task1 = [&count]()->tinycoro::Task<int32_t>
    {
        co_return ++count; 
    };

    auto task2 = [&count]()->tinycoro::Task<int32_t>
    {
        co_return ++count; 
    };

    auto task3 = [&count]()->tinycoro::Task<int32_t>
    {
        co_return ++count; 
    };

    auto task4 = [&count]()->tinycoro::Task<int32_t>
    {
        co_return ++count; 
    };

    auto task5 = [&count]()->tinycoro::Task<int32_t>
    {
        co_return ++count; 
    };

    auto [v1, v2, v3, v4, v5] = tinycoro::RunInline(task1(), task2(), task3(), task4(), task5());
    
    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 2);
    EXPECT_EQ(v3, 3);
    EXPECT_EQ(v4, 4);
    EXPECT_EQ(v5, count);
}

TEST(RunInlineTest, RunInline_FunctionalTest_4)
{
    int32_t count{};

    auto task = [&count]()->tinycoro::Task<int32_t>
    {
        co_return ++count; 
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task());
    tasks.push_back(task());
    tasks.push_back(task());
    tasks.push_back(task());
    tasks.push_back(task());

    auto results = tinycoro::RunInline(tasks);
    
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    EXPECT_EQ(results[2], 3);
    EXPECT_EQ(results[3], 4);
    EXPECT_EQ(results[4], count);
}

TEST(RunInlineTest, RunInline_FunctionalTest_5)
{
    int32_t count{};

    auto task1 = [&count]()->tinycoro::Task<bool>
    {
        EXPECT_EQ(count++, 0);

        co_return true; 
    };

    auto task2 = [&count]()->tinycoro::Task<uint32_t>
    {
        EXPECT_EQ(count++, 1);

        co_return 42u; 
    };

    auto task3 = [&count]()->tinycoro::Task<void>
    {
        EXPECT_EQ(count++, 2);

        co_return; 
    };

    auto [v1, v2, v3] = tinycoro::RunInline(task1(), task2(), task3());
    
    EXPECT_TRUE((std::same_as<decltype(v1), bool>));
    EXPECT_TRUE((std::same_as<decltype(v2), uint32_t>));
    EXPECT_TRUE((std::same_as<decltype(v3), tinycoro::VoidType>));

    EXPECT_EQ(count, 3);
}

TEST(RunInlineTest, RunInline_FunctionalTest_exception)
{
    int32_t count{};

    auto task1 = [&count]()->tinycoro::Task<bool>
    {
        EXPECT_EQ(count++, 0);

        co_return true; 
    };

    auto task2 = [&count]()->tinycoro::Task<uint32_t>
    {
        EXPECT_EQ(count++, 1);

        throw std::runtime_error("Error");  // this throws an exception

        co_return 42u; 
    };

    auto task3 = [&count]()->tinycoro::Task<void>
    {
        EXPECT_EQ(count++, 2);

        co_return; 
    };

    auto func = [&]{std::ignore = tinycoro::RunInline(task1(), task2(), task3()); };

    EXPECT_THROW(func(), std::runtime_error);
    EXPECT_EQ(count, 2);
}

TEST(RunInlineTest, RunInline_FunctionalTest_exception2)
{
    int32_t count{};

    auto task = [&count]()->tinycoro::Task<int32_t>
    {
        if(count > 2)
        {
            throw std::runtime_error{"Error"}; // throw an exception
        }

        co_return ++count; 
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task());
    tasks.push_back(task());
    tasks.push_back(task());
    tasks.push_back(task());
    tasks.push_back(task());

    auto func = [&]{std::ignore = tinycoro::RunInline(tasks); };

    EXPECT_THROW(func(), std::runtime_error);
    EXPECT_EQ(3, count);
}