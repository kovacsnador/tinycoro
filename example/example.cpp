#include <iostream>
#include <coroutine>
#include <ranges>
#include <cstdint>
#include <chrono>
#include <future>
#include <syncstream>

#include "Scheduler.hpp"
#include "CoroTask.hpp"
#include "Generator.hpp"
#include "AsyncCallbackAwaiter.hpp"

using namespace std::chrono_literals;

auto SyncOut(std::ostream& stream = std::cout)
{
    return std::osyncstream{stream};
}

/*template <typename RepT, typename PeriodT = std::ratio<1>>
struct SleepAwaiter
{
    SleepAwaiter(std::chrono::duration<RepT, PeriodT> duration)
    : _duration{std::move(duration)}
    {
    }

    tinycoro::Task<void> Sleep()
    {
        auto start = std::chrono::system_clock::now();

        while (std::chrono::system_clock::now() - start < _duration)
        {
            co_await std::suspend_always{};
        }
    }

private:
    std::chrono::duration<RepT, PeriodT> _duration;
};*/

/*tinycoro::Task<void> Sleep(auto duration)
{
    auto start = std::chrono::system_clock::now();

    while (std::chrono::system_clock::now() - start < duration)
    {
        co_await std::suspend_always{};
    }
}*/

/*typedef void (*funcPtr)(void*, int);

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

tinycoro::Task<void> AsyncCallback()
{
    SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

    auto cb
        = [](void* userData, int i) { SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n'; };

    auto asyncCallback = [](auto cbWithNotify) { return AsyncCallbackAPI(nullptr, cbWithNotify); };

    auto async1 = tinycoro::AsyncCallbackAwaiter{asyncCallback, cb};
    auto async2 = tinycoro::AsyncCallbackAwaiter{asyncCallback, cb};

    if (std::same_as<decltype(async1), decltype(async2)>)
    {
        SyncOut() << "Same Type\n";
    }
    else
    {
        SyncOut() << "NOT a same Type\n";
    }

    int  index{43};
    auto cb2 = [&index](int i) { SyncOut() << "  Callback called... " << index << " Thread id: " << std::this_thread::get_id() << '\n'; };

    // wait with return value
    auto jthread = co_await tinycoro::AsyncCallbackAwaiter{[](auto cbWithNotify) { return AsyncCallbackAPI(nullptr, cbWithNotify); }, cb};

    // wait without return value
    co_await tinycoro::AsyncCallbackAwaiter{[](auto cbWithNotify) { return AsyncCallbackAPIvoid(cbWithNotify); }, cb2};

    co_return;
}*/

/*tinycoro::Task<int32_t> Calculate(int32_t i)
{
    SyncOut() << "  Calculate... Thread id: " << std::this_thread::get_id() << '\n';
    auto future = std::async(
        std::launch::async,
        [](auto i) {
            std::this_thread::sleep_for(1s);
            return i * i;
        },
        i);
    while (future.wait_for(0s) != std::future_status::ready)
    {
        co_await std::suspend_always{};
    }

    auto res = future.get();

    SyncOut() << "  Calculate return: " << res << " , Thread id : " << std::this_thread::get_id() << '\n';
    co_return res;
}

tinycoro::Task<int32_t> Print()
{
    SyncOut() << "  Print1... Thread id: " << std::this_thread::get_id() << '\n';
    co_await std::suspend_always{};
    SyncOut() << "  Print2... Thread id: " << std::this_thread::get_id() << '\n';

    auto calcCoro = Calculate(2);
    auto val      = co_await calcCoro;

    // auto val = calcCoro.hdl.promise().ReturnValue();
    SyncOut() << "  Print3 val: " << val << ", Thread id : " << std::this_thread::get_id() << '\n';

    co_return val;
}

tinycoro::Task<int32_t> DoWork()
{
    auto start = std::chrono::system_clock::now();

    int32_t val{};

    // co_await Print();

    while (std::chrono::system_clock::now() - start < 1s)
    {
        SyncOut() << "  DoWork... Thread id: " << std::this_thread::get_id() << '\n';

        auto coro = Print();

        val += co_await coro;

        std::this_thread::sleep_for(500ms);
    }

    co_return val;
}

tinycoro::Task<void> PrintVoid()
{
    SyncOut() << "  PrintVoid 1... Thread id: " << std::this_thread::get_id() << '\n';
    co_await std::suspend_always{};
    SyncOut() << "  PrintVoid 2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

tinycoro::Task<void> PrintVoidSub()
{
    SyncOut() << "  PrintVoidSub 1... Thread id: " << std::this_thread::get_id() << '\n';
    co_await PrintVoid();
    SyncOut() << "  PrintVoidSub 2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

tinycoro::Task<void> DoWorkVoid()
{
    auto start = std::chrono::system_clock::now();

    int32_t val{};

    co_await PrintVoid();

    while (std::chrono::system_clock::now() - start < 1s)
    {
        SyncOut() << "  DoWork... Thread id: " << std::this_thread::get_id() << '\n';

        auto printCoro = Print();
        val += co_await printCoro;

        SyncOut() << "  DoWork value: " << val << " ... Thread id : " << std::this_thread::get_id() << '\n';

        std::this_thread::sleep_for(500ms);
    }

    co_return;
}

tinycoro::Task<int32_t> SimpleWork42()
{
    SyncOut() << "  SimpleWork42... Thread id: " << std::this_thread::get_id() << '\n';

    // std::this_thread::sleep_for(1s);

    // co_yield 41;

    // co_await std::suspend_always{};

    // throw std::runtime_error("SimpleWork exception");

    co_return 42;
}*/

/*struct OnlyMoveable2
{
    OnlyMoveable(int ii)
    : i{ii}
    {
    }

    OnlyMoveable(OnlyMoveable&& other)            = default;
    OnlyMoveable& operator=(OnlyMoveable&& other) = default;

    int i;
};

tinycoro::Task<OnlyMoveable2> WorkOnlyMoveable()
{
    SyncOut() << "  WorkOnlyMoveable... Thread id: " << std::this_thread::get_id() << '\n';

    // std::this_thread::sleep_for(1s);

    // co_yield 41;

    // co_await std::suspend_always{};

    throw std::runtime_error("WorkOnlyMoveable exception");

    co_return 42;
}

tinycoro::Task<void> SimpleWork()
{
    SyncOut() << "  SimpleWork... Thread id: " << std::this_thread::get_id() << '\n';

    co_await Sleep(1s);

    SyncOut() << "  SimpleWork after sleep... Thread id: " << std::this_thread::get_id() << '\n';

    // throw std::runtime_error("SimpleWork exception");

    auto val = co_await SimpleWork42();

    co_return;
}*/

/*tinycoro::CoroTaskYieldReturn<int32_t, double> SimpleWorkYieldReturnValue()
{
    SyncOut() << "  SimpleWork... Thread id: " << std::this_thread::get_id() << '\n';

    co_yield 41;

    co_return 42.0;
}

tinycoro::CoroTaskYield<int32_t> Test3()
{
    SyncOut() << "  Test3... Thread id: " << std::this_thread::get_id() << '\n';
    co_yield 40;
    co_yield 41;
    co_yield 42;
    SyncOut() << "  Test4... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}*/

/*tinycoro::Task<void> Test1()
{
    SyncOut() << "  Test1... Thread id: " << std::this_thread::get_id() << '\n';

    auto coro = Test3();

    while (coro.resume() != tinycoro::ECoroResumeState::DONE)
    {
        auto yield = coro.hdl.promise().YieldValue();
        SyncOut() << "  Test3() yield " << yield << " ... Thread id : " << std::this_thread::get_id() << '\n';
        co_await std::suspend_always{};
    }

    SyncOut() << "  Test2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}*/

/*struct S
{
    S(int32_t ii)
    : i{ii}
    {
        SyncOut() << "  S() " << i << " ... Thread id : " << std::this_thread::get_id() << '\n';
    }

    ~S() { SyncOut() << "  ~S() " << i << " ... Thread id: " << std::this_thread::get_id() << '\n'; }

    int32_t i;
};

struct PromiseTest1
{
};

struct PromiseTest2
{
    std::vector<int> v;
};

struct PromiseTest3
{
    int64_t          ii;
    int64_t          ir;
    int64_t          iu;
    int64_t          io;
    std::vector<int> v;
};

struct PromiseTest4
{
    std::array<int, 100> arr;
};*/

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

        auto cb = [](void* userData, int i) {
            SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';
        };

        // wait without return value
        auto jthread = co_await tinycoro::AsyncCallbackAwaiter{[](auto cbWithNotify) { return AsyncCallbackAPI(nullptr, cbWithNotify); }, cb};
        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    SyncOut() << "co_return => " << future.get() << '\n';
}

int main()
{
    /*std::cout << "sizeof(std::coroutine_handle<>): " << sizeof(std::coroutine_handle<>) << '\n';
    std::cout << "sizeof(std::coroutine_handle<PromiseTest1>): " << sizeof(std::coroutine_handle<PromiseTest1>) << '\n';
    std::cout << "sizeof(std::coroutine_handle<PromiseTest2>): " << sizeof(std::coroutine_handle<PromiseTest2>) << '\n';
    std::cout << "sizeof(std::coroutine_handle<PromiseTest3>): " << sizeof(std::coroutine_handle<PromiseTest3>) << '\n';
    std::cout << "sizeof(std::coroutine_handle<PromiseTest4>): " << sizeof(std::coroutine_handle<PromiseTest4>) << '\n';

    std::cout << "sizeof(std::promise<void>): " << sizeof(std::promise<void>) << '\n';
    std::cout << "sizeof(std::promise<int>): " << sizeof(std::promise<int>) << '\n';
    std::cout << "sizeof(std::promise<bool>): " << sizeof(std::promise<bool>) << '\n';
    std::cout << "sizeof(std::promise<PromiseTest1>): " << sizeof(std::promise<PromiseTest1>) << '\n';
    std::cout << "sizeof(std::promise<PromiseTest2>): " << sizeof(std::promise<PromiseTest2>) << '\n';
    std::cout << "sizeof(std::promise<PromiseTest3>): " << sizeof(std::promise<PromiseTest3>) << '\n';
    std::cout << "sizeof(std::promise<PromiseTest4>): " << sizeof(std::promise<PromiseTest4>) << '\n';

    std::cout << "sizeof(FutureState<void>): " << sizeof(tinycoro::FutureState<void>) << '\n';
    std::cout << "sizeof(FutureState<int>): " << sizeof(tinycoro::FutureState<int>) << '\n';
    std::cout << "sizeof(FutureState<bool>): " << sizeof(tinycoro::FutureState<bool>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest1>): " << sizeof(tinycoro::FutureState<PromiseTest1>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest2>): " << sizeof(tinycoro::FutureState<PromiseTest2>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest3>): " << sizeof(tinycoro::FutureState<PromiseTest3>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest4>): " << sizeof(tinycoro::FutureState<PromiseTest4>) << '\n';*/

    tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};
    {
        Example_voidTask(scheduler);

        Example_returnValueTask(scheduler);

        Example_moveOnlyValue(scheduler);

        Example_aggregateValue(scheduler);

        Example_exception(scheduler);

        Example_nestedTask(scheduler);

        Example_nestedException(scheduler);

        Example_generator(scheduler);

        Example_multiTasks(scheduler);

        Example_multiTaskDifferentValues(scheduler);

        Example_sleep(scheduler);

        Example_asyncPulling(scheduler);

        Example_asyncCallbackAwaiter(scheduler);

        Example_asyncCallbackAwaiterWithReturnValue(scheduler);

        // auto voidWorkFut = scheduler.Enqueue(SimpleWork());

        // voidWorkFut.Get();

        /*auto simpleWorkCoro = SimpleWorkYieldReturnValue();

        scheduler.Enqueue(CoroTaskView{simpleWorkCoro.hdl});*/

        // auto futureSW = scheduler.Enqueue(WorkOnlyMoveable());
        //  auto futureVoid = scheduler.Enqueue(PrintVoid());

        /*try
        {
            futureSW.get();
        }
        catch (const std::exception& ex)
        {
            SyncOut() << ex.what() << '\n';
        }*/

        // futureVoid.get();

        // auto workAsyncFut = scheduler.Enqueue(SimpleWork42());*/

        /*auto futAsync = scheduler.Enqueue(AsyncCallback());

        futAsync.get();*/

        // auto val = workAsyncFut.Get();*/

        // auto simpleWorkTasks = scheduler.EnqueueTasks(SimpleWork(), SimpleWork(), SimpleWork());

        /*std::get<0>(simpleWorkTasks).get();
        std::get<1>(simpleWorkTasks).get();
        std::get<2>(simpleWorkTasks).get();*/

        // tinycoro::WaitAll(simpleWorkTasks);

        /*auto mixedTasks = scheduler.EnqueueTasks(SimpleWork(), SimpleWork42(), SimpleWork42(), SimpleWork());

        auto results = tinycoro::WaitAll(mixedTasks);

        SyncOut() << "result 1: " << std::get<1>(results) << " result 2: " << std::get<2>(results) << '\n';*/

        // auto value = futureSW->Get();

        // scheduler.Wait();

        // SyncOut() << "SimpleWorkYieldReturnValue yieldValue: " << simpleWorkCoro.hdl.promise().yieldValue << "\n";
        // SyncOut() << "SimpleWorkYieldReturnValue returnValue: " << simpleWorkCoro.hdl.promise().returnValue << "\n";

        /*auto generator = [](int32_t max) -> Generator<S> {
            for (auto it : std::views::iota(0, max))
            {
                co_yield S{ it };
            }
        };


        for (const auto& it : generator(12))
        {
            //TD<decltype(it)> td;

            SyncOut() << it.i << '\n';
        }*/
    }

    return 0;
}
