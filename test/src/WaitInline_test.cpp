#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <array>

#include <tinycoro/tinycoro_all.h>

struct WaitInline_PauseHandlerMock
{
    WaitInline_PauseHandlerMock() = default;
    WaitInline_PauseHandlerMock(tinycoro::ResumeCallback_t c)
    : cb{c}
    {
    }

    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(bool, IsCancellable, (), (const));

    tinycoro::ResumeCallback_t cb;
};

template<typename T>
struct PromiseMock
{
    using value_type = T;
};

template<typename ReturnT, typename PauseHandlerT>
struct WaitInline_TaskMock
{
    using value_type = ReturnT;

    MOCK_METHOD(ReturnT, await_resume, ());
    MOCK_METHOD(std::shared_ptr<PauseHandlerT>, GetPauseHandler, ());
    MOCK_METHOD(std::shared_ptr<PauseHandlerT>, SetPauseHandler, (tinycoro::ResumeCallback_t));
    MOCK_METHOD(void, SetStopSource, (std::stop_source));
    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(tinycoro::ETaskResumeState, ResumeState, ());
    MOCK_METHOD(bool, IsPaused, (), (const, noexcept));
    MOCK_METHOD(bool, IsCancelled, (), (const, noexcept));
    MOCK_METHOD(bool, IsDone, (), (const, noexcept));

    std::shared_ptr<PauseHandlerT> pauseHandlerMock;
};

template<typename ReturnT, typename PauseHandlerT>
struct WaitInline_TaskMockWrapper
{
    using value_type = ReturnT;
    using promise_type = PromiseMock<ReturnT>;

    WaitInline_TaskMockWrapper()
    : mock{std::make_shared<WaitInline_TaskMock<ReturnT, PauseHandlerT>>()}
    {
        ON_CALL(*mock, IsDone).WillByDefault(::testing::Return(true));
    }

    ReturnT await_resume()
    {
        return mock->await_resume();
    }

    std::shared_ptr<PauseHandlerT> GetPauseHandler()
    {
        return mock->GetPauseHandler();

    }

    std::shared_ptr<PauseHandlerT> SetPauseHandler(tinycoro::ResumeCallback_t cb)
    {
        return mock->SetPauseHandler(cb);
    }

    void SetStopSource(auto stopSource)
    {
        mock->SetStopSource(stopSource);
    }

    void Resume()
    {
        mock->Resume();
    }

    tinycoro::ETaskResumeState ResumeState()
    {
        return mock->ResumeState();
    }

    bool IsPaused() const noexcept
    {
        return mock->IsPaused();
    }

    bool IsCancelled() const noexcept
    {
        return mock->IsCancelled();
    }

    bool IsDone() const noexcept
    {
        return mock->IsDone();
    }

    std::shared_ptr<WaitInline_TaskMock<ReturnT, PauseHandlerT>> mock;
};


TEST(WaitInlineTest, WaitInlineTest_void)
{
    WaitInline_TaskMockWrapper<void, WaitInline_PauseHandlerMock> mock;

    EXPECT_CALL(*mock.mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto callback) {
            mock.mock->pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>(callback);
            return mock.mock->pauseHandlerMock;
        }
    ));

    EXPECT_CALL(*mock.mock, Resume()).Times(1);
    EXPECT_CALL(*mock.mock, IsPaused()).Times(1);
    EXPECT_CALL(*mock.mock, IsDone()).WillOnce(testing::Return(false)).WillOnce(testing::Return(true));
    EXPECT_CALL(*mock.mock, ResumeState()).WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

 
    tinycoro::AllOf(mock);
}

TEST(WaitInlineTest, WaitInlineTest_int32)
{
    WaitInline_TaskMockWrapper<int32_t, WaitInline_PauseHandlerMock> mock;

    EXPECT_CALL(*mock.mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto callback) {
            mock.mock->pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>(callback);
            return mock.mock->pauseHandlerMock;
        }
    ));

    EXPECT_CALL(*mock.mock, Resume()).Times(1);
    EXPECT_CALL(*mock.mock, IsPaused()).Times(1);

    EXPECT_CALL(*mock.mock, IsDone())
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true));
    
    EXPECT_CALL(*mock.mock, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));
    
    EXPECT_CALL(*mock.mock, await_resume())
        .Times(1)
        .WillOnce(testing::Return(42));

    auto val = tinycoro::AllOf(mock);
    EXPECT_EQ(42, val);
}

TEST(WaitInlineTest, WaitInlineTest_pause)
{
    WaitInline_TaskMock<int32_t, WaitInline_PauseHandlerMock> mock;

    EXPECT_CALL(mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto callback) {
            mock.pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>(callback);

            EXPECT_CALL(*mock.pauseHandlerMock, Resume()).Times(1);

            return mock.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock, GetPauseHandler()).WillOnce(testing::Invoke(
        [&mock] () {
            return mock.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock, IsPaused())
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(false));

    EXPECT_CALL(mock, Resume()).Times(1);

    EXPECT_CALL(mock, IsDone())
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(mock, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::PAUSED))
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

    EXPECT_CALL(mock, await_resume()).Times(1).WillOnce(testing::Return(42));

    auto resumer = [&]()->tinycoro::Task<void> { mock.pauseHandlerMock->cb(tinycoro::ENotifyPolicy::RESUME); co_return;};

    auto [val, voidValue] = tinycoro::AllOf(mock, resumer());
    EXPECT_EQ(42, val);
}

TEST(WaitInlineTest, WaitInlineTest_cancelled)
{
    WaitInline_TaskMockWrapper<int32_t, WaitInline_PauseHandlerMock> mock;

    EXPECT_CALL(*mock.mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock] (auto callback) {
            mock.mock->pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>(callback);
            return mock.mock->pauseHandlerMock;
        }
    ));

    EXPECT_CALL(*mock.mock, Resume()).Times(1);
    EXPECT_CALL(*mock.mock, IsPaused()).Times(1);

    EXPECT_CALL(*mock.mock, IsDone())
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(false));
    
    EXPECT_CALL(*mock.mock, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::STOPPED));

    EXPECT_CALL(*mock.mock, await_resume()).Times(0);
 
    auto val = tinycoro::AllOf(mock);
    EXPECT_TRUE((std::same_as<decltype(val), std::optional<int32_t>>));
    EXPECT_FALSE(val.has_value());
}

TEST(WaitInlineTest, WaitInlineTest_multiTasks)
{
    WaitInline_TaskMock<int32_t, WaitInline_PauseHandlerMock> mock1;

    mock1.pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>();

    EXPECT_CALL(mock1, IsDone)
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(mock1, IsPaused)
        .WillRepeatedly(testing::Return(false));

    EXPECT_CALL(mock1, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock1] (auto) {
            return mock1.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock1, Resume()).Times(1);
    EXPECT_CALL(mock1, ResumeState())
        .WillRepeatedly(testing::Return(tinycoro::ETaskResumeState::DONE));

    EXPECT_CALL(mock1, await_resume()).Times(1).WillOnce(testing::Return(42));

    WaitInline_TaskMock<int32_t, WaitInline_PauseHandlerMock> mock2;

    mock2.pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>();

    EXPECT_CALL(mock2, IsDone)
        .WillOnce(testing::Return(false))
        .WillOnce(testing::Return(true))
        .WillOnce(testing::Return(true));

    EXPECT_CALL(mock2, IsPaused)
        .WillRepeatedly(testing::Return(false));

    EXPECT_CALL(mock2, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&mock2] (auto) {
            return mock2.pauseHandlerMock;
        }
    ));

    EXPECT_CALL(mock2, Resume()).Times(1);
    EXPECT_CALL(mock2, ResumeState())
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::SUSPENDED))
        .WillOnce(testing::Return(tinycoro::ETaskResumeState::STOPPED));

    EXPECT_CALL(mock2, await_resume()).Times(1).WillOnce(testing::Return(43));

    auto [result1, result2] = tinycoro::AllOf(mock1, mock2);
    EXPECT_EQ(42, result1);
    EXPECT_EQ(43, result2);
}

TEST(WaitInlineTest, WaitInlineTest_dynamicTasks)
{
    std::vector<WaitInline_TaskMockWrapper<int32_t, WaitInline_PauseHandlerMock>> tasks;

    for(size_t i=0; i < 10; ++i)
    {
        tasks.emplace_back();

        tasks[i].mock->pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>();

        EXPECT_CALL(*tasks[i].mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&tasks, index = i] (auto) {
            return tasks[index].mock->pauseHandlerMock;
        }
        ));

        EXPECT_CALL(*tasks[i].mock, Resume()).Times(1);
        EXPECT_CALL(*tasks[i].mock, IsPaused()).Times(1);
        
        EXPECT_CALL(*tasks[i].mock, IsDone())
            .WillOnce(testing::Return(false))
            .WillOnce(testing::Return(true));
        
        EXPECT_CALL(*tasks[i].mock, ResumeState())
            .WillOnce(testing::Return(tinycoro::ETaskResumeState::DONE));

        EXPECT_CALL(*tasks[i].mock, await_resume()).Times(1).WillOnce(testing::Return(42));
    }

    auto results = tinycoro::AllOf(tasks);
    std::ranges::for_each(results, [](const auto& v) {
        EXPECT_EQ(42, v);
    });
}

TEST(WaitInlineTest, WaitInlineTest_dynamicTasks_cancelled)
{
    std::vector<WaitInline_TaskMockWrapper<int32_t, WaitInline_PauseHandlerMock>> tasks;

    for(size_t i=0; i < 10; ++i)
    {
        tasks.emplace_back();

        tasks[i].mock->pauseHandlerMock = std::make_shared<WaitInline_PauseHandlerMock>();

        EXPECT_CALL(*tasks[i].mock, SetPauseHandler(testing::_)).WillOnce(testing::Invoke(
        [&tasks, index = i] (auto) {
            return tasks[index].mock->pauseHandlerMock;
        }
        ));

        EXPECT_CALL(*tasks[i].mock, Resume()).Times(1);
        EXPECT_CALL(*tasks[i].mock, IsPaused()).Times(1);
        
        EXPECT_CALL(*tasks[i].mock, IsDone())
            .WillOnce(testing::Return(false))
            .WillOnce(testing::Return(false));
        
        EXPECT_CALL(*tasks[i].mock, ResumeState())
            .WillOnce(testing::Return(tinycoro::ETaskResumeState::STOPPED));

        EXPECT_CALL(*tasks[i].mock, await_resume()).Times(0);
    }

    auto results = tinycoro::AllOf(tasks);

    std::ranges::for_each(results, [](const auto& v) {
        EXPECT_FALSE(v.has_value());
    });
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_voidTasks)
{
    size_t i = 0;

    auto consumer1 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 0);
        co_return; 
    };

    auto consumer2 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 1);
        co_return; 
    };

    auto consumer3 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 2);
        co_return; 
    };

    tinycoro::AllOf(consumer1(), consumer2(), consumer3());
    EXPECT_EQ(i, 3);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_1)
{
    tinycoro::SingleEvent<int32_t> event;

    auto consumer = [&event]()->tinycoro::Task<int32_t>
    {
        co_return co_await event; 
    };

    // set the value
    event.Set(42);

    auto value = tinycoro::AllOf(consumer());
    EXPECT_EQ(value, 42);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_2)
{
    tinycoro::SoftClock clock;
    tinycoro::SingleEvent<int32_t> event;

    auto consumer = [&event]()->tinycoro::Task<int32_t>
    {
        co_return co_await event; 
    };

    auto producer = [&event, &clock]()->tinycoro::Task<>
    {
        co_await tinycoro::SleepFor(clock, 100ms);
        event.Set(42);
    };

    tinycoro::Scheduler scheduler{1};

    std::ignore = scheduler.Enqueue(producer());

    auto value = tinycoro::AllOf(consumer());
    EXPECT_EQ(value, 42);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_3)
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

    auto [v1, v2, v3, v4, v5] = tinycoro::AllOf(task1(), task2(), task3(), task4(), task5());
    
    EXPECT_EQ(v1, 1);
    EXPECT_EQ(v2, 2);
    EXPECT_EQ(v3, 3);
    EXPECT_EQ(v4, 4);
    EXPECT_EQ(v5, count);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_4)
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

    auto results = tinycoro::AllOf(tasks);
    
    EXPECT_EQ(results[0], 1);
    EXPECT_EQ(results[1], 2);
    EXPECT_EQ(results[2], 3);
    EXPECT_EQ(results[3], 4);
    EXPECT_EQ(results[4], count);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_5)
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

    auto task3 = [&count]()->tinycoro::Task<>
    {
        EXPECT_EQ(count++, 2);

        co_return; 
    };

    auto [v1, v2, v3] = tinycoro::AllOf(task1(), task2(), task3());
    
    EXPECT_TRUE((std::same_as<decltype(v1), std::optional<bool>>));
    EXPECT_TRUE((std::same_as<decltype(v2), std::optional<uint32_t>>));
    EXPECT_TRUE((std::same_as<decltype(v3), std::optional<tinycoro::VoidType>>));

    EXPECT_EQ(count, 3);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_exception)
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

    auto task3 = [&count]()->tinycoro::Task<>
    {
        EXPECT_EQ(count++, 2);

        co_return; 
    };

    auto func = [&]{std::ignore = tinycoro::AllOf(task1(), task2(), task3()); };

    EXPECT_THROW(func(), std::runtime_error);
    EXPECT_EQ(count, 3);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_exception2)
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

    auto func = [&]{std::ignore = tinycoro::AllOf(tasks); };

    EXPECT_THROW(func(), std::runtime_error);
    EXPECT_EQ(3, count);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_pauseTask)
{
    tinycoro::Latch latch{2};

    size_t i = 0;

    auto consumer1 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 0);
        co_await latch.ArriveAndWait();
        EXPECT_EQ(i++, 5);
    };

    auto consumer2 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 1);
        co_await latch.ArriveAndWait();
        EXPECT_EQ(i++, 2);
    };

    auto consumer3 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 3);
        co_await latch.ArriveAndWait();
        EXPECT_EQ(i++, 4);
    };

    tinycoro::AllOf(consumer1(), consumer2(), consumer3());
    EXPECT_EQ(i, 6);
}

TEST(WaitInlineTest, WaitInline_FunctionalTest_pauseTask_stoped)
{
    std::stop_source ssource;

    tinycoro::Latch latch{2};

    size_t i = 0;

    auto consumer1 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 0);
        co_await latch.ArriveAndWait();
        EXPECT_EQ(i++, 3);
    };

    auto consumer2 = [&]()->tinycoro::Task<>
    {
        EXPECT_EQ(i++, 1);
        co_await latch.ArriveAndWait();

        // request a stop
        ssource.request_stop();

        EXPECT_EQ(i++, 2);
    };

    auto consumer3 = [&]()->tinycoro::Task<>
    {
        // this task should be stopped through stopsource
        co_await tinycoro::this_coro::yield_cancellable();

        // This code should never reached...
        i++; 
    };

    auto task3 = consumer3();
    task3.SetStopSource(ssource);

    tinycoro::AllOf(consumer1(), consumer2(), std::move(task3));
    EXPECT_EQ(i, 4);
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_with_scheduler)
{   
    tinycoro::SoftClock clock;
    tinycoro::Scheduler scheduler;

    tinycoro::Barrier barrier{4};

    auto task = [&barrier](int32_t r) -> tinycoro::Task<int32_t> {
        co_await barrier.ArriveAndWait();
        co_return r;
    };

    auto deferedTask = [&task, &clock](int32_t r) -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock,200ms);
        co_return co_await task(r);
    };

    // simulate parallel tasks
    auto [fut1, fut2] = scheduler.Enqueue(deferedTask(40), deferedTask(43));

    // Run intline the 2 task which are notified by other scheduler
    auto [ret1, ret2] = tinycoro::AllOf(task(41), task(42));

    EXPECT_EQ(ret1, 41);
    EXPECT_EQ(ret2, 42);

    // wait for the other tasks to finish
    EXPECT_EQ(fut1.get(), 40);
    EXPECT_EQ(fut2.get(), 43);
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_sleep)
{
    tinycoro::SoftClock clock;
    auto deferedTask = [&clock](int32_t r) -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 200ms);
        co_return r;
    };

    auto start = tinycoro::SoftClock::Now();
    auto res = tinycoro::AllOf(deferedTask(42));

    EXPECT_TRUE(tinycoro::SoftClock::Now() >= start + 200ms);
    EXPECT_EQ(res, 42);
    
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_sleepMulti)
{
    tinycoro::SoftClock clock;
    auto deferedTask = [&clock](int32_t r) -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock, 200ms);
        co_return r;
    };

    auto start = tinycoro::SoftClock::Now();
    auto [r1, r2, r3] = tinycoro::AllOf(deferedTask(41), deferedTask(42), deferedTask(43));

    // tinycoro::Sleep is running async so the only guarantie that it takes longer then 200ms
    EXPECT_TRUE(tinycoro::SoftClock::Now() >= start + 200ms);
    EXPECT_EQ(r1, 41);
    EXPECT_EQ(r2, 42);
    EXPECT_EQ(r3, 43);
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_sleepMulti_dynamic)
{
    tinycoro::SoftClock clock;

    auto deferedTask = [&clock](int32_t r) -> tinycoro::Task<int32_t> {
        co_await tinycoro::SleepFor(clock,200ms);
        co_return r;
    };

    std::array<tinycoro::Task<int32_t>, 3> tasks{deferedTask(41), deferedTask(42), deferedTask(43)};

    auto start = tinycoro::SoftClock::Now();
    auto results = tinycoro::AllOf(tasks);

    // tinycoro::Sleep is running async so the only guarantie that it takes longer then 200ms
    EXPECT_TRUE(tinycoro::SoftClock::Now() >= start + 200ms);
    EXPECT_EQ(results[0], 41);
    EXPECT_EQ(results[1], 42);
    EXPECT_EQ(results[2], 43);
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_pushawait)
{
    tinycoro::BufferedChannel<int32_t> channel;

    auto task1 = [&]() -> tinycoro::Task<int32_t> {

        auto consumer = [&]()-> tinycoro::Task<int32_t> {
            int32_t val;
            std::ignore = co_await channel.PopWait(val);
            co_return val;
        };

        auto producer = [&]()-> tinycoro::Task<> {
            co_await channel.PushWait(42);
        };

        auto [val1, val2] = tinycoro::AllOf(consumer(), producer());

        EXPECT_TRUE((std::same_as<decltype(val2), std::optional<tinycoro::VoidType>>));
        co_return val1.value();
    };

    auto fortyTwo = tinycoro::AllOf(task1());
    EXPECT_EQ(fortyTwo, 42);
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_cancelled)
{
    tinycoro::SoftClock clock;

    tinycoro::AutoEvent event;

    auto waitTask = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(event.Wait());
        co_return 42;
    };

    auto sleepTask = [&]() -> tinycoro::Task<void> {
        co_await tinycoro::SleepFor(clock, 100ms);
    };

    auto [r1, r2, r3, r4, r5, r6] = tinycoro::AnyOf(waitTask(), waitTask(), waitTask(), waitTask(), waitTask(), sleepTask());

    // task should be cancelled
    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());

    EXPECT_TRUE(r6.has_value());

    event.Set();
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_cancelled_latch)
{
    tinycoro::SoftClock clock;

    tinycoro::Latch latch{1};

    auto waitTask = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(latch.Wait());
        co_return 42;
    };

    auto sleepTask = [&]() -> tinycoro::Task<void> {
        co_await tinycoro::SleepFor(clock, 100ms);
    };

    auto [r1, r2, r3, r4, r5, r6] = tinycoro::AnyOf(waitTask(), waitTask(), waitTask(), waitTask(), waitTask(), sleepTask());

    // task should be cancelled
    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());

    EXPECT_TRUE(r6.has_value());
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_cancelled_dynamic)
{
    tinycoro::SoftClock clock;
    tinycoro::AutoEvent event;

    auto waitTask = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(event.Wait());
        co_return 42;
    };

    auto sleepTask = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::SleepFor(clock, 100ms);
        co_return 44;
    };

    std::vector<tinycoro::TaskNIC<int32_t>> tasks;
    for(size_t i = 0; i < 5; ++i)
    {
        tasks.emplace_back(waitTask());
    }

    tasks.emplace_back(sleepTask());

    auto results = tinycoro::AnyOf(tasks);

    // task should be cancelled
    EXPECT_FALSE(results[0].has_value());
    EXPECT_FALSE(results[1].has_value());
    EXPECT_FALSE(results[2].has_value());
    EXPECT_FALSE(results[3].has_value());
    EXPECT_FALSE(results[4].has_value());

    EXPECT_EQ(results[5].value(), 44);

    event.Set();
}

TEST(WaitInlineTest, WaitInlineTest_FunctionalTest_cancelled_manual)
{
    tinycoro::SoftClock clock;

    tinycoro::ManualEvent event;

    auto waitTask = [&]() -> tinycoro::TaskNIC<int32_t> {
        co_await tinycoro::Cancellable(event.Wait());
        co_return 42;
    };

    auto sleepTask = [&]() -> tinycoro::Task<void> {
        co_await tinycoro::SleepFor(clock, 100ms);
    };

    auto [r1, r2, r3, r4, r5, r6] = tinycoro::AnyOf(waitTask(), waitTask(), waitTask(), waitTask(), waitTask(), sleepTask());

    // task should be cancelled
    EXPECT_FALSE(r1.has_value());
    EXPECT_FALSE(r2.has_value());
    EXPECT_FALSE(r3.has_value());
    EXPECT_FALSE(r4.has_value());
    EXPECT_FALSE(r5.has_value());

    event.Set();
}