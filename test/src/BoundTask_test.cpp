#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <set>
#include <string>

#include <tinycoro/tinycoro_all.h>

TEST(BoundTaskTest, BoundTaskTest_make)
{
    auto task = []() -> tinycoro::Task<std::string> { co_return "42"; };

    auto taskWrapper = tinycoro::MakeBound(task);

    EXPECT_TRUE((std::same_as<tinycoro::Task<std::string>, decltype(taskWrapper)>));
}

struct TaskWrapperMockImpl
{
    struct PauseHandlerMock
    {
    };

    MOCK_METHOD(void, await_ready, ());
    MOCK_METHOD(void, await_resume, ());
    MOCK_METHOD(bool, await_suspend, (std::coroutine_handle<>));

    MOCK_METHOD(void, Resume, ());
    MOCK_METHOD(bool, IsDone, ());
    MOCK_METHOD(tinycoro::ETaskResumeState, ResumeState, ());
    MOCK_METHOD(PauseHandlerMock, SetPauseHandler, (std::function<void()>));
    MOCK_METHOD(PauseHandlerMock, GetPauseHandler, (), (noexcept));
    MOCK_METHOD(void, SetStopSource, (std::stop_source));
    MOCK_METHOD(void, SetCurrentAwaitable, (void*));
    MOCK_METHOD(void*, Address, (), (const noexcept));
    MOCK_METHOD(void*, Release, (), (noexcept));
};

template<typename T>
struct PromiseTypeMock
{
    using value_type = T;
};

template<typename T>
struct TaskWrapperMock
{
    using promise_type = PromiseTypeMock<T>;
    using value_type = T;

    auto await_ready() { return impl->await_ready(); }

    [[nodiscard]] auto await_resume() { return impl->await_resume(); }

    auto await_suspend(auto hdl) { return impl->await_suspend(hdl); }

    void Resume() { impl->Resume(); }

    bool IsDone() { return impl->IsDone(); }

    [[nodiscard]] auto ResumeState() { return impl->ResumeState(); }

    auto SetPauseHandler(auto pauseResume) { return impl->SetPauseHandler(std::move(pauseResume)); }

    auto GetPauseHandler() noexcept { return impl->GetPauseHandler(); }

    template <typename U>
    void SetStopSource(U&& arg)
    {
        impl->SetStopSource(std::forward<U>(arg));
    }

    template <typename U>
    void SetCurrentAwaitable(U&& cb)
    {
        impl->SetCurrentAwaitable(std::forward<U>(cb));
    }

    [[nodiscard]] auto Address() const noexcept { return impl->Address(); }

    [[nodiscard]] auto Release() noexcept { return impl->Release(); }

    std::shared_ptr<TaskWrapperMockImpl> impl = std::make_shared<TaskWrapperMockImpl>();
};

TEST(BoundTaskTest, BoundTaskFunctionalTest_SingleBoundTask)
{
    tinycoro::Scheduler scheduler{4};

    int32_t i{};

    auto coro = [&i]() -> tinycoro::Task<int32_t> { co_return i++; };

    auto result = tinycoro::AllOf(scheduler, tinycoro::MakeBound(coro));

    EXPECT_EQ(result, 0);
    EXPECT_EQ(i, 1);
}

TEST(BoundTaskTest, BoundTaskFunctionalTest_coawait_task)
{
    tinycoro::Scheduler scheduler{4};

    int32_t i{};

    auto coro = [&i]() -> tinycoro::Task<int32_t> {
        auto coro2 = [&]() -> tinycoro::Task<int32_t> { co_return ++i; };

        auto val = co_await tinycoro::MakeBound(coro2);
        EXPECT_EQ(val, 1);

        co_return ++val;
    };

    auto result = tinycoro::AllOf(scheduler, tinycoro::MakeBound(coro));

    EXPECT_EQ(result, 2);
    EXPECT_EQ(i, 1);
}

TEST(BoundTaskTest, BoundTaskFunctionalTest_destructed_coroFunction)
{
    tinycoro::Scheduler scheduler;

    int32_t i{};

    std::future<std::optional<int32_t>> future;

    {
        auto coro = [&i]() -> tinycoro::Task<int32_t> { co_return ++i; };
        future = scheduler.Enqueue(tinycoro::MakeBound(coro));
    }

    auto result = future.get();

    EXPECT_EQ(result, 1);
    EXPECT_EQ(i, 1);
}

TEST(BoundTaskTest, BoundTaskFunctionalTest_destructed_coroFunction_safe)
{
    tinycoro::Scheduler scheduler;

    int32_t i{};

    std::future<std::optional<int32_t>> future;

    {
        auto coro = [](auto& i) -> tinycoro::Task<int32_t> { co_return ++i; };
        future = scheduler.Enqueue(coro(i));
    }

    auto result = future.get();

    EXPECT_EQ(result, 1);
    EXPECT_EQ(i, 1);
}

TEST(BoundTaskTest, BoundTaskFunctionalTest_lambda_immediately_invoked)
{
    tinycoro::Scheduler scheduler;

    int32_t i{};

    // immediately invoked lambda (prvalue) function
    auto future = scheduler.Enqueue(tinycoro::MakeBound([&i]() -> tinycoro::Task<int32_t> { co_return ++i; }));

    EXPECT_EQ(future.get(), 1);
    EXPECT_EQ(i, 1);
}

struct BoundTaskTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(BoundTaskTest, BoundTaskTest, testing::Values(1, 10, 100, 1000));

TEST_P(BoundTaskTest, BoundTaskFunctionalTest_MultiTasks)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    std::atomic<int32_t> i{};

    auto coro = [&i]() -> tinycoro::Task<int32_t> { co_return ++i; };

    using BoundTaskT = decltype(tinycoro::MakeBound(coro));
    std::vector<BoundTaskT> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(tinycoro::MakeBound(coro));
    }

    auto results = tinycoro::AllOf(scheduler, std::move(tasks));

    // check for unique values
    std::set<size_t> set;
    for (auto it : results)
    {
        // no lock needed here only one consumer
        auto [_, inserted] = set.insert(*it);
        EXPECT_TRUE(inserted);
    }

    EXPECT_EQ(count, i);
}

TEST_P(BoundTaskTest, BoundTaskFunctionalTest_coawait_task_multi)
{
    const auto count = GetParam();

    tinycoro::Scheduler scheduler;

    std::atomic<int32_t> i{};

    auto coro = [&]() -> tinycoro::Task<int32_t> {
        auto coro2 = [&]() -> tinycoro::Task<int32_t> {
            co_await std::suspend_always{};
            co_return ++i;
        };

        co_return co_await tinycoro::MakeBound(coro2);
    };

    using BoundTaskT = decltype(tinycoro::MakeBound(coro));
    std::vector<BoundTaskT> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(tinycoro::MakeBound(coro));
    }

    auto results = tinycoro::AllOf(scheduler, std::move(tasks));

    // check for unique values
    std::set<size_t> set;
    for (auto it : results)
    {
        // no lock needed here only one consumer
        auto [_, inserted] = set.insert(*it);
        EXPECT_TRUE(inserted);
    }

    EXPECT_EQ(count, i);
}