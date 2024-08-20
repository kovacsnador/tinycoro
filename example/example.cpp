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

typedef void(*funcPtr)(void*, int);

std::jthread AsyncCallbackAPI(void* userData, funcPtr cb)
{
    return std::jthread{ [cb, userData] {

        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(1s);
        cb(userData, 42);
    } };
}

void AsyncCallbackAPIvoid(std::regular_invocable<int> auto cb)
{
    std::jthread t{ [cb] {

        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(1s);
        cb(42);
    } };
    t.detach();
}

template<typename... T>
class TD;

CoroTaskVoid AsyncCallback()
{
    SyncOut() << "  AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

    auto cb = [](void* userData, int i) {
        SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';
    };

    auto asyncCallback = [](auto cbWithNotify) { return AsyncCallbackAPI(nullptr, cbWithNotify); };

    auto async1 = AsyncCallbackAwaiter{asyncCallback, cb};
    auto async2 = AsyncCallbackAwaiter{asyncCallback, cb};

    if (std::same_as<decltype(async1), decltype(async2)>)
    {
        SyncOut() << "Same Type\n";
    }
    else
    {
        SyncOut() << "NOT a same Type\n";
    }

    int index{43};
    auto cb2 = [&index](int i) {
        SyncOut() << "  Callback called... " << index << " Thread id: " << std::this_thread::get_id() << '\n';
    };

    // wait with return value
    auto jthread = co_await AsyncCallbackAwaiter{[](auto cbWithNotify) { return AsyncCallbackAPI(nullptr, cbWithNotify); }, cb};

    // wait without return value
    co_await AsyncCallbackAwaiter{[](auto cbWithNotify) { return AsyncCallbackAPIvoid(cbWithNotify); }, cb2};

    co_return;
}

CoroTaskReturn<int32_t> Calculate(int32_t i)
{
    SyncOut() << "  Calculate... Thread id: " << std::this_thread::get_id() << '\n';
    auto future = std::async(std::launch::async, [](auto i) { std::this_thread::sleep_for(1s); return i * i; }, i);
    while (future.wait_for(0s) != std::future_status::ready)
    {
        co_await std::suspend_always{};
    }

    auto res = future.get();

    SyncOut() << "  Calculate return: " << res << " , Thread id : " << std::this_thread::get_id() << '\n';
    co_return res;
}


CoroTaskReturn<int32_t> Print()
{
    SyncOut() << "  Print1... Thread id: " << std::this_thread::get_id() << '\n';
    co_await std::suspend_always{};
    SyncOut() << "  Print2... Thread id: " << std::this_thread::get_id() << '\n';

    auto calcCoro = Calculate(2);
    auto val = co_await calcCoro;

    //auto val = calcCoro.hdl.promise().ReturnValue();
    SyncOut() << "  Print3 val: " << val << ", Thread id : " << std::this_thread::get_id() << '\n';

    co_return val;
}

CoroTaskReturn<int32_t> DoWork()
{
    auto start = std::chrono::system_clock::now();

    int32_t val{};

    //co_await Print();

    while (std::chrono::system_clock::now() - start < 1s)
    {
        SyncOut() << "  DoWork... Thread id: " << std::this_thread::get_id() << '\n';

        auto coro = Print();

        val += co_await coro;

        std::this_thread::sleep_for(500ms);
    }

    co_return val;
}

CoroTaskVoid PrintVoid()
{
    SyncOut() << "  PrintVoid 1... Thread id: " << std::this_thread::get_id() << '\n';
    co_await std::suspend_always{};
    SyncOut() << "  PrintVoid 2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

CoroTaskVoid PrintVoidSub()
{
    SyncOut() << "  PrintVoidSub 1... Thread id: " << std::this_thread::get_id() << '\n';
    co_await PrintVoid();
    SyncOut() << "  PrintVoidSub 2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

CoroTaskVoid DoWorkVoid()
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

CoroTaskReturn<int32_t> SimpleWork42()
{
    SyncOut() << "  SimpleWork42... Thread id: " << std::this_thread::get_id() << '\n';

    std::this_thread::sleep_for(1s);

    //co_yield 41;

    co_await std::suspend_always{};

    //throw std::runtime_error("SimpleWork exception");

    co_return 42;
}

CoroTaskVoid SimpleWork()
{
    SyncOut() << "  SimpleWork... Thread id: " << std::this_thread::get_id() << '\n';

    std::this_thread::sleep_for(1s);

    //throw std::runtime_error("SimpleWork exception");

    auto val = co_await SimpleWork42();

    co_return;
}

CoroTaskYieldReturn<int32_t, double> SimpleWorkYieldReturnValue()
{
    SyncOut() << "  SimpleWork... Thread id: " << std::this_thread::get_id() << '\n';

    co_yield 41;

    co_return 42.0;
}

CoroTaskYield<int32_t> Test3()
{
    SyncOut() << "  Test3... Thread id: " << std::this_thread::get_id() << '\n';
    co_yield 40;
    co_yield 41;
    co_yield 42;
    SyncOut() << "  Test4... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

CoroTaskVoid Test1()
{
    SyncOut() << "  Test1... Thread id: " << std::this_thread::get_id() << '\n';

    auto coro = Test3();

    while (coro.resume() != ECoroResumeState::DONE)
    {
        auto yield = coro.hdl.promise().YieldValue();
        SyncOut() << "  Test3() yield " << yield << " ... Thread id : " << std::this_thread::get_id() << '\n';
        co_await std::suspend_always{};
    }

    SyncOut() << "  Test2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

struct S
{
    S(int32_t ii)
        : i{ii}
    {
        SyncOut() << "  S() " << i << " ... Thread id : " << std::this_thread::get_id() << '\n';
    }

    ~S()
    {
        SyncOut() << "  ~S() " << i << " ... Thread id: " << std::this_thread::get_id() << '\n';
    }

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
    int64_t ii;
    int64_t ir;
    int64_t iu;
    int64_t io;
    std::vector<int> v;
};

struct PromiseTest4
{
    std::array<int, 100> arr;
};

int main()
{
    std::cout << "sizeof(std::coroutine_handle<>): " << sizeof(std::coroutine_handle<>) << '\n';
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

    std::cout << "sizeof(FutureState<void>): " << sizeof(FutureState<void>) << '\n';
    std::cout << "sizeof(FutureState<int>): " << sizeof(FutureState<int>) << '\n';
    std::cout << "sizeof(FutureState<bool>): " << sizeof(FutureState<bool>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest1>): " << sizeof(FutureState<PromiseTest1>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest2>): " << sizeof(FutureState<PromiseTest2>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest3>): " << sizeof(FutureState<PromiseTest3>) << '\n';
    std::cout << "sizeof(FutureState<PromiseTest4>): " << sizeof(FutureState<PromiseTest4>) << '\n';


    CoroScheduler scheduler{std::thread::hardware_concurrency()};
    {
        //auto voidWorkFut = scheduler.Enqueue(SimpleWork());

        //voidWorkFut.Get();

        /*auto simpleWorkCoro = SimpleWorkYieldReturnValue();

        scheduler.Enqueue(CoroTaskView{simpleWorkCoro.hdl});*/

        auto futureSW = scheduler.Enqueue(SimpleWork());
        //auto futureVoid = scheduler.Enqueue(PrintVoid());

        try
        {
            futureSW.get();
        }
        catch (const std::exception& ex)
        {
            SyncOut() << ex.what() << '\n';
        }
        
        //futureVoid.get();

        //auto workAsyncFut = scheduler.Enqueue(SimpleWork42());*/

        auto futAsync = scheduler.Enqueue(AsyncCallback());

        futAsync.get();

        //auto val = workAsyncFut.Get();*/

        auto test1Tasks = scheduler.EnqueueTasks(Test1(), Test1(), Test1());

        /*std::get<0>(test1Tasks).get();
        std::get<1>(test1Tasks).get();
        std::get<2>(test1Tasks).get();*/

        WaitAll(test1Tasks);


        auto mixedTasks = scheduler.EnqueueTasks(Test1(), SimpleWork42(), SimpleWork42(), Test1());

        auto results = WaitAll(mixedTasks);

        SyncOut() << "result 1: " << std::get<1>(results) << " result 2: " << std::get<2>(results) << '\n';
 
        //auto value = futureSW->Get();

        //scheduler.Wait();


        //SyncOut() << "SimpleWorkYieldReturnValue yieldValue: " << simpleWorkCoro.hdl.promise().yieldValue << "\n";
        //SyncOut() << "SimpleWorkYieldReturnValue returnValue: " << simpleWorkCoro.hdl.promise().returnValue << "\n";



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
