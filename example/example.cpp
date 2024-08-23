#include <iostream>
#include <coroutine>
#include <ranges>
#include <cstdint>
#include <chrono>
#include <future>
#include <syncstream>
#include <stop_token>

#include "tinycoro/tinycoro_all.h"

using namespace std::chrono_literals;

auto SyncOut(std::ostream& stream = std::cout)
{
    return std::osyncstream{stream};
}

void Example_voidTask(auto& scheduler)
{
    SyncOut() << "\n\nExample_voidTask:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto future = scheduler.Enqueue(task());
    future.get();

    SyncOut() << "co_return => void" << '\n';
}

void Example_taskView(auto& scheduler)
{
    SyncOut() << "\n\nExample_taskView:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto coro   = task();
    auto future = scheduler.Enqueue(coro.task_view());

    future.get();

    SyncOut() << "co_return => void" << '\n';
}

void Example_returnValueTask(auto& scheduler)
{
    SyncOut() << "\n\nExample_returnValueTask:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    auto val    = future.get();

    SyncOut() << "co_return => " << val << '\n';
}

void Example_moveOnlyValue(auto& scheduler)
{
    SyncOut() << "\n\nExample_moveOnlyValue:\n";

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

    auto task = []() -> tinycoro::Task<OnlyMoveable> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    auto val    = future.get();

    SyncOut() << "co_return => " << val.i << '\n';
}

void Example_aggregateValue(auto& scheduler)
{
    SyncOut() << "\n\nExample_aggregateValue:\n";

    struct Aggregate
    {
        int32_t i;
        int32_t j;
    };

    auto task = []() -> tinycoro::Task<Aggregate> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        co_return Aggregate{42, 43};
    };

    auto future = scheduler.Enqueue(task());
    auto val    = future.get();

    SyncOut() << "co_return => " << val.i << " " << val.j << '\n';
}

void Example_nestedTask(auto& scheduler)
{
    SyncOut() << "\n\nExample_nestedTask:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            SyncOut() << "    Nested Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
            co_return 42;
        };

        auto val = co_await nestedTask();

        co_return val;
    };

    auto future = scheduler.Enqueue(task());

    SyncOut() << "co_return => " << future.get() << '\n';
}

void Example_exception(auto& scheduler)
{
    SyncOut() << "\n\nExample_exception:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        throw std::runtime_error("Example_exception exception");

        co_return;
    };

    auto future = scheduler.Enqueue(task());

    try
    {
        future.get();
    }
    catch (const std::exception& e)
    {
        SyncOut() << e.what() << '\n';
    }
}

void Example_nestedException(auto& scheduler)
{
    SyncOut() << "\n\nExample_nestedException:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            SyncOut() << "    Nested Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

            throw std::runtime_error("Example_nestedException nested exception");

            co_return 42;
        };

        auto val = co_await nestedTask();

        co_return val;
    };

    auto future = scheduler.Enqueue(task());

    try
    {
        auto val = future.get();
        SyncOut() << "co_return => " << val << '\n';
    }
    catch (const std::exception& e)
    {
        SyncOut() << e.what() << '\n';
    }
}

void Example_generator(auto& scheduler)
{
    SyncOut() << "\n\nExample_generator:\n";

    struct S
    {
        int32_t i;
    };

    auto generator = [](int32_t max) -> tinycoro::Generator<S> {
        SyncOut() << "  Coro generator..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        for (auto it : std::views::iota(0, max))
        {
            SyncOut() << "  Yield value: " << it << "  Thread id : " << std::this_thread::get_id() << '\n';
            co_yield S{it};
        }
    };

    for (const auto& it : generator(12))
    {
        SyncOut() << "Generator Value: " << it.i << '\n';
    }
}

void Example_multiTasks(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiTasks:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto futures = scheduler.EnqueueTasks(task(), task(), task());
    tinycoro::WaitAll(futures);

    SyncOut() << "WaitAll co_return => void" << '\n';
}

void Example_multiMovedTasksDynamic(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiMovedTasksDynamic:\n";

    auto task1 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 41;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 42;
    };

    auto task3 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 43;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto futures = scheduler.EnqueueTasks(std::move(tasks));
    auto results = tinycoro::WaitAll(futures);

    SyncOut() << "WaitAll co_return => " << results[0] << ", " << results[1] << ", " << results[2] << '\n';
}

void Example_multiTasksDynamic(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiTasksDynamic:\n";

    auto task1 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 41;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 42;
    };

    auto task3 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 43;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    auto futures = scheduler.EnqueueTasks(tasks);
    auto results = tinycoro::WaitAll(futures);

    SyncOut() << "WaitAll co_return => " << results[0] << ", " << results[1] << ", " << results[2] << '\n';
}

void Example_multiTaskDifferentValues(auto& scheduler)
{
    SyncOut() << "\n\nExample_multiTaskDifferentValues:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 42;
    };

    struct S
    {
        int32_t i;
    };

    auto task3 = []() -> tinycoro::Task<S> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        co_return 43;
    };

    auto futures = scheduler.EnqueueTasks(task1(), task2(), task3());
    auto results = tinycoro::WaitAll(futures);

    auto monostate = std::get<0>(results);
    auto val       = std::get<1>(results);
    auto s         = std::get<2>(results);

    SyncOut() << std::boolalpha << "WaitAll task1 co_return => void " << std::is_same_v<std::monostate, decltype(monostate)> << '\n';
    SyncOut() << "WaitAll task2 co_return => " << val << '\n';
    SyncOut() << "WaitAll task3 co_return => " << s.i << '\n';
}

void Example_sleep(auto& scheduler)
{
    SyncOut() << "\n\nExample_sleep:\n";

    auto sleep = [](auto duration) -> tinycoro::Task<int32_t> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await std::suspend_always{};
        }
        co_return 42;
    };

    auto task = [&sleep]() -> tinycoro::Task<int32_t> {
        auto val = co_await sleep(1s);
        co_return val;

        // co_return co_await sleep(1s);     // or write short like this
    };

    auto future = scheduler.Enqueue(task());

    SyncOut() << "co_return => " << future.get() << '\n';
}

void Example_asyncPulling(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncPulling:\n";

    auto asyncTask = [](int32_t i) -> tinycoro::Task<int32_t> {
        SyncOut() << "  asyncTask... Thread id: " << std::this_thread::get_id() << '\n';
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

        SyncOut() << "  asyncTask return: " << res << " , Thread id : " << std::this_thread::get_id() << '\n';
        co_return res;
    };

    auto task = [&asyncTask]() -> tinycoro::Task<int32_t> {
        SyncOut() << "  task... Thread id: " << std::this_thread::get_id() << '\n';
        auto val = co_await asyncTask(4);
        co_return val;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

typedef void (*funcPtr)(void*, int);

std::jthread AsyncCallbackAPI(void* userData, funcPtr cb)
{
    return std::jthread{[cb, userData] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(1s);
        cb(userData, 42);
    }};
}

void AsyncCallbackAPIvoid(std::regular_invocable<int> auto cb)
{
    std::jthread t{[cb] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(1s);
        cb(42);
    }};
    t.detach();
}

void Example_asyncCallbackAwaiter(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter:\n";

    auto task = []() -> tinycoro::Task<void> {
        SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        auto cb = [](int i) { SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n'; };

        // wait without return value
        co_await tinycoro::AsyncCallbackAwaiter{[](auto cbWithNotify) { AsyncCallbackAPIvoid(cbWithNotify); }, cb};
    };

    auto future = scheduler.Enqueue(task());
    future.get();

    SyncOut() << "co_return => void" << '\n';
}

void Example_asyncCallbackAwaiterWithReturnValue(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiterWithReturnValue:\n";

    auto task = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        auto cb = [](void* userData, int i) { SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n'; };

        // wait with return value
        auto jthread = co_await tinycoro::AsyncCallbackAwaiter{[](auto cbWithNotify) { return AsyncCallbackAPI(nullptr, cbWithNotify); }, cb};
        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

void Example_usageWithStopToken(auto& scheduler)
{
    SyncOut() << "\n\nExample_usageWithStopToken:\n";

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

    auto futures = scheduler.EnqueueTasks(task1(1s, source), task2(source.get_token()));

    auto results = tinycoro::WaitAll(futures);

    SyncOut() << "co_return => " << std::get<1>(results) << '\n';
}

int main()
{
    tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};
    {
        Example_voidTask(scheduler);

        Example_taskView(scheduler);

        Example_returnValueTask(scheduler);

        Example_moveOnlyValue(scheduler);

        Example_aggregateValue(scheduler);

        Example_exception(scheduler);

        Example_nestedTask(scheduler);

        Example_nestedException(scheduler);

        Example_generator(scheduler);

        Example_multiTasks(scheduler);

        Example_multiMovedTasksDynamic(scheduler);

        Example_multiTasksDynamic(scheduler);

        Example_multiTaskDifferentValues(scheduler);

        Example_sleep(scheduler);

        Example_asyncPulling(scheduler);

        Example_asyncCallbackAwaiter(scheduler);

        Example_asyncCallbackAwaiterWithReturnValue(scheduler);

        Example_usageWithStopToken(scheduler);
    }

    return 0;
}
