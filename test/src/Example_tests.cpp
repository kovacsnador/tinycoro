#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>
#include <example/Common.h>

struct ExampleTest : public testing::Test
{
    tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};
};

TEST_F(ExampleTest, Example_voidTaskTest)
{
    auto task = []() -> tinycoro::Task<void> { co_return; };

    auto future = scheduler.Enqueue(task());
    EXPECT_NO_THROW(future.get());
}

TEST_F(ExampleTest, Example_taskView)
{
    auto task = []() -> tinycoro::Task<void> { co_return; };

    auto coro   = task();
    auto future = scheduler.Enqueue(coro.TaskView());

    EXPECT_NO_THROW(future.get());
}

TEST_F(ExampleTest, Example_returnValueTask)
{
    auto task = []() -> tinycoro::Task<int32_t> { co_return 42; };

    auto future = scheduler.Enqueue(task());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ExampleTest, Example_moveOnlyValue)
{
    struct OnlyMoveable
    {
        OnlyMoveable(int32_t ii)
        : i{ii}
        {
        }

        OnlyMoveable(OnlyMoveable&& other)            = default;
        OnlyMoveable& operator=(OnlyMoveable&& other) = default;

        int32_t i;
    };

    auto task = []() -> tinycoro::Task<OnlyMoveable> { co_return 42; };

    auto future = scheduler.Enqueue(task());
    auto val    = future.get();

    EXPECT_EQ(val.i, 42);
}

TEST(ExampleTestFutureState, Example_moveOnlyValue_FutureState)
{
    tinycoro::CoroThreadPool<tinycoro::PackagedTask<>, tinycoro::FutureState> scheduler{4};

    struct OnlyMoveable
    {
        OnlyMoveable(int32_t ii)
        : i{ii}
        {
        }

        OnlyMoveable(OnlyMoveable&& other)            = default;
        OnlyMoveable& operator=(OnlyMoveable&& other) = default;

        int32_t i;
    };

    auto task = []() -> tinycoro::Task<OnlyMoveable> { co_return 42; };

    auto future = scheduler.Enqueue(task());
    auto val    = future.get();

    EXPECT_EQ(val.i, 42);
}

TEST_F(ExampleTest, Example_aggregateValue)
{
    struct Aggregate
    {
        int32_t i;
        int32_t j;
    };

    auto task = []() -> tinycoro::Task<Aggregate> { co_return Aggregate{42, 43}; };

    auto future = scheduler.Enqueue(task());
    auto val    = future.get();

    EXPECT_EQ(val.i, 42);
    EXPECT_EQ(val.j, 43);
}

TEST_F(ExampleTest, Example_exception)
{

    auto task = []() -> tinycoro::Task<void> {
        throw std::runtime_error("Example_exception exception");
        co_return;
    };

    auto future = scheduler.Enqueue(task());

    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST_F(ExampleTest, Example_nestedTask)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        auto nestedTask = []() -> tinycoro::Task<int32_t> { co_return 42; };

        auto val = co_await nestedTask();

        EXPECT_EQ(val, 42);

        co_return val;
    };

    auto future = scheduler.Enqueue(task());

    EXPECT_EQ(future.get(), 42);
}

TEST_F(ExampleTest, Example_nestedException)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            throw std::runtime_error("Example_nestedException nested exception");
            co_return 42;
        };

        auto val = co_await nestedTask();
        co_return val;
    };

    auto future = scheduler.Enqueue(task());
    EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(Example_generator, Example_generator)
{
    struct S
    {
        int32_t i;
    };

    auto generator = [](int32_t max) -> tinycoro::Generator<S> {
        for (auto it : std::views::iota(0, max))
        {
            co_yield S{it};
        }
    };

    size_t i{0};
    for (const auto& it : generator(12))
    {
        EXPECT_EQ(i++, it.i);
    }
}

TEST_F(ExampleTest, Example_multiTasks)
{
    auto task = []() -> tinycoro::Task<void> { co_return; };

    auto futures = scheduler.EnqueueTasks(task(), task(), task());
    EXPECT_NO_THROW(tinycoro::GetAll(futures));
}

TEST_F(ExampleTest, Example_multiMovedTasksDynamic)
{

    auto task1 = []() -> tinycoro::Task<int32_t> { co_return 41; };

    auto task2 = []() -> tinycoro::Task<int32_t> { co_return 42; };

    auto task3 = []() -> tinycoro::Task<int32_t> { co_return 43; };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto futures = scheduler.EnqueueTasks(std::move(tasks));
    auto results = tinycoro::GetAll(futures);

    EXPECT_EQ(results[0], 41);
    EXPECT_EQ(results[1], 42);
    EXPECT_EQ(results[2], 43);
}

TEST_F(ExampleTest, Example_multiMovedTasksDynamicvoid)
{

    auto task1 = []() -> tinycoro::Task<void> { co_return; };

    auto task2 = []() -> tinycoro::Task<void> { co_return; };

    auto task3 = []() -> tinycoro::Task<void> { co_return; };

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto futures = scheduler.EnqueueTasks(std::move(tasks));
    EXPECT_NO_THROW(tinycoro::GetAll(futures));
}

TEST_F(ExampleTest, Example_multiTasksDynamic)
{
    auto task1 = []() -> tinycoro::Task<int32_t> { co_return 41; };

    auto task2 = []() -> tinycoro::Task<int32_t> { co_return 42; };

    auto task3 = []() -> tinycoro::Task<int32_t> { co_return 43; };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto futures = scheduler.EnqueueTasks(tasks);
    auto results = tinycoro::GetAll(futures);

    EXPECT_EQ(results[0], 41);
    EXPECT_EQ(results[1], 42);
    EXPECT_EQ(results[2], 43);
}

TEST_F(ExampleTest, Example_multiTaskDifferentValues)
{
    auto task1 = []() -> tinycoro::Task<void> { co_return; };

    auto task2 = []() -> tinycoro::Task<int32_t> { co_return 42; };

    struct S
    {
        int32_t i;
    };

    auto task3 = []() -> tinycoro::Task<S> { co_return 43; };

    auto futures = scheduler.EnqueueTasks(task1(), task2(), task3());

    auto results = tinycoro::GetAll(futures);

    auto voidType = std::get<0>(results);

    EXPECT_TRUE((std::same_as<tinycoro::VoidType, decltype(voidType)>));
    EXPECT_EQ(std::get<1>(results), 42);
    EXPECT_EQ(std::get<2>(results).i, 43);
}

TEST_F(ExampleTest, Example_multiTaskDifferentValuesExpection)
{
    auto task1 = []() -> tinycoro::Task<void> { co_return; };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        throw std::runtime_error("Exception throwed!");
        co_return 42;
    };

    struct S
    {
        int32_t i;
    };

    auto task3 = []() -> tinycoro::Task<S> { co_return 43; };

    auto futures = scheduler.EnqueueTasks(task1(), task2(), task3());
    EXPECT_THROW(tinycoro::GetAll(futures), std::runtime_error);
}

TEST_F(ExampleTest, Example_sleep)
{

    auto sleep = [](auto duration) -> tinycoro::Task<int32_t> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await std::suspend_always{};
        }
        co_return 42;
    };

    auto task = [&sleep]() -> tinycoro::Task<int32_t> {
        auto val = co_await sleep(100ms);
        co_return val;

        // co_return co_await sleep(1s);     // or write short like this
    };

    auto future = scheduler.Enqueue(task());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ExampleTest, Example_asyncPulling)
{
    auto asyncTask = [](int32_t i) -> tinycoro::Task<int32_t> {
        auto future = std::async(
            std::launch::async,
            [](auto i) {
                // simulate some work
                std::this_thread::sleep_for(1s);
                return i * i;
            },
            i);

        // simple old school pulling
        while (future.wait_for(0s) != std::future_status::ready)
        {
            co_await std::suspend_always{};
        }

        auto res = future.get();
        co_return res;
    };

    auto task = [&asyncTask]() -> tinycoro::Task<int32_t> {
        auto val = co_await asyncTask(4);

        EXPECT_EQ(val, 16);

        co_return val;
    };

    auto future = scheduler.Enqueue(task());
    EXPECT_EQ(future.get(), 16);
}

TEST_F(ExampleTest, Example_asyncCallbackAwaiter)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        auto cb = []([[maybe_unused]] void* userData, [[maybe_unused]] int i) {
            // do some work
            std::this_thread::sleep_for(100ms);
        };

        // wait with return value
        auto val = co_await tinycoro::AsyncCallbackAwaiter(
            [](auto wrappedCallback) {
                AsyncCallbackAPIvoid(wrappedCallback, nullptr);
                return 44;
            },
            cb);

        EXPECT_EQ(val, 44);

        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ExampleTest, Example_asyncCallbackAwaiter_CStyle)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        auto cb = [](void* userData, [[maybe_unused]] int i) {
            auto d = static_cast<int32_t*>(userData);
            *d     = 21;
        };

        auto async = [](auto wrappedCallback, void* wrappedUserData) {
            AsyncCallbackAPIvoid(wrappedCallback, wrappedUserData);
            return 21;
        };

        int userData{0};

        auto res = co_await tinycoro::AsyncCallbackAwaiter_CStyle(async, cb, tinycoro::IndexedUserData<0>(&userData));

        EXPECT_EQ(userData, 21);
        EXPECT_EQ(res, 21);

        co_return userData + res;
    };

    auto future = scheduler.Enqueue(task());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ExampleTest, Example_asyncCallbackAwaiter_CStyleVoid)
{
    auto task1 = []() -> tinycoro::Task<void> {
        auto task2 = []() -> tinycoro::Task<void> {
            auto cb = [](void* userData, [[maybe_unused]] int i) {
                auto null = static_cast<std::nullptr_t*>(userData);
                EXPECT_EQ(null, nullptr);
            };

            co_await tinycoro::AsyncCallbackAwaiter_CStyle(
                [](auto cb, auto userData) { AsyncCallbackAPIvoid(cb, userData); }, cb, tinycoro::IndexedUserData<0>(nullptr));
        };

        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());
    EXPECT_NO_THROW(future.get());
}

TEST_F(ExampleTest, Example_asyncCallbackAwaiterWithReturnValue)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        struct S
        {
            int i{41};
        };

        auto cb = [](void* userData, [[maybe_unused]] int i, [[maybe_unused]] int j) {
            auto* s = static_cast<S*>(userData);
            s->i++;

            // do some work
            std::this_thread::sleep_for(100ms);
        };

        S s;

        auto asyncCb = [](auto cb, auto userData) { return AsyncCallbackAPI(userData, cb); };

        // wait with return value
        auto jthread = co_await tinycoro::AsyncCallbackAwaiter_CStyle(asyncCb, cb, tinycoro::IndexedUserData<0>(&s));

        EXPECT_TRUE((std::same_as<std::jthread, decltype(jthread)>));

        co_return s.i;
    };

    auto future = scheduler.Enqueue(task());
    EXPECT_EQ(future.get(), 42);
}

TEST_F(ExampleTest, Example_usageWithStopToken)
{
    auto task1 = [](auto duration, std::stop_source& source) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await std::suspend_always{};
        }
        source.request_stop();
    };

    auto task2 = [](std::stop_token token) -> tinycoro::Task<int32_t> {
        auto sleep = [](auto duration) -> tinycoro::Task<void> {
            for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
            {
                co_await std::suspend_always{};
            }
        };

        int32_t result{};
        while (token.stop_requested() == false)
        {
            ++result;
            co_await sleep(100ms);
        }
        co_return result;
    };

    std::stop_source source;

    auto futures = scheduler.EnqueueTasks(task1(300ms, source), task2(source.get_token()));

    auto results = tinycoro::GetAll(futures);

    auto task2Val = std::get<1>(results);

    EXPECT_TRUE((std::same_as<int32_t, decltype(task2Val)>));
}

TEST_F(ExampleTest, Example_AnyOfVoid)
{
    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend<void>{};
        }
    };

    std::stop_source source;
    EXPECT_NO_THROW(tinycoro::AnyOfWithStopSource(scheduler, source, task1(1s), task1(2s), task1(3s)));
}

TEST_F(ExampleTest, Example_AnyOf)
{
    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    auto results = tinycoro::AnyOf(scheduler, task1(1s), task1(2s), task1(3s));

    auto t1 = std::get<0>(results);
    auto t2 = std::get<1>(results);
    auto t3 = std::get<2>(results);

    EXPECT_TRUE((std::same_as<int32_t, decltype(t1)>));
    EXPECT_TRUE((std::same_as<int32_t, decltype(t2)>));
    EXPECT_TRUE((std::same_as<int32_t, decltype(t3)>));

    EXPECT_TRUE(t1 > 0);
    EXPECT_TRUE(t2 > 0);
    EXPECT_TRUE(t3 > 0);
}

TEST_F(ExampleTest, Example_AnyOfDynamic)
{
    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1(1s));
    tasks.push_back(task1(2s));
    tasks.push_back(task1(3s));

    auto results = tinycoro::AnyOf(scheduler, tasks);

    EXPECT_TRUE(results[0] > 0);
    EXPECT_TRUE(results[1] > 0);
    EXPECT_TRUE(results[2] > 0);
}

TEST_F(ExampleTest, Example_AnyOfDynamicVoid)
{
    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        [[maybe_unused]] auto stopToken  = co_await tinycoro::StopTokenAwaiter{};
        [[maybe_unused]] auto stopSource = co_await tinycoro::StopSourceAwaiter{};

        co_await tinycoro::Sleep(duration);
    };

    std::stop_source source;

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task1(10ms));
    tasks.push_back(task1(20ms));
    tasks.push_back(task1(30ms));

    EXPECT_NO_THROW(tinycoro::AnyOfWithStopSource(scheduler, source, tasks));
}

TEST_F(ExampleTest, Example_AnyOfVoidException)
{
    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend<void>{};
        }
    };

    auto task2 = [](auto duration) -> tinycoro::Task<int32_t> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            throw std::runtime_error("Exception throwed!");
            co_await tinycoro::CancellableSuspend<void>{};
        }
        co_return 42;
    };

    EXPECT_THROW(tinycoro::AnyOf(scheduler, task1(1s), task1(2s), task1(3s), task2(5s)), std::runtime_error);
}

struct CustomAwaiter
{
    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(auto hdl) noexcept
    {
        // save resume task callback
        _resumeTask = tinycoro::PauseHandler::PauseTask(hdl);

        auto cb = [](void* userData, [[maybe_unused]] int i) {
            auto self = static_cast<decltype(this)>(userData);

            // do some work
            std::this_thread::sleep_for(100ms);
            self->_userData++;

            // resume the coroutine (you need to make them exception safe)
            self->_resumeTask();
        };

        AsyncCallbackAPIvoid(cb, this);
    }

    constexpr auto await_resume() const noexcept { return _userData; }

    int32_t _userData{41};

    std::function<void()> _resumeTask;
};

TEST_F(ExampleTest, ExampleOwnAwaiter)
{
    auto asyncTask = []() -> tinycoro::Task<int32_t> {
        // do some work before

        auto val = co_await CustomAwaiter{};

        // do some work after
        co_return val;
    };

    auto future = scheduler.Enqueue(asyncTask());
    auto val    = tinycoro::GetAll(future);

    EXPECT_EQ(val, 42);
}