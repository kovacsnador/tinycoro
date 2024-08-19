#include <iostream>
#include <coroutine>
#include <ranges>
#include <cstdint>
#include <chrono>
#include <future>
#include <syncstream>

#include "Scheduler.hpp"
#include "CoroTask.hpp"

using namespace std::chrono_literals;

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
    co_await calcCoro;

    auto val = calcCoro.hdl.promise().returnValue;
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

        co_await coro;
        val += coro.hdl.promise().returnValue;

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
        co_await printCoro;

        val += printCoro.hdl.promise().returnValue;

        SyncOut() << "  DoWork value: " << val << " ... Thread id : " << std::this_thread::get_id() << '\n';


        std::this_thread::sleep_for(500ms);
    }

    co_return;
}

CoroTaskReturn<int32_t> SimpleWork()
{
    SyncOut() << "  SimpleWork... Thread id: " << std::this_thread::get_id() << '\n';

    co_return 42;
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
        auto yield = coro.hdl.promise().yieldValue;
        SyncOut() << "  Test3() yield " << yield << " ... Thread id : " << std::this_thread::get_id() << '\n';
        co_await std::suspend_always{};
    }

    SyncOut() << "  Test2... Thread id: " << std::this_thread::get_id() << '\n';
    co_return;
}

int main()
{
    CoroScheduler scheduler{std::thread::hardware_concurrency()};
    {
        /*scheduler.Enqueue(DoWorkVoid());

        auto simpleWorkCoro = SimpleWorkYieldReturnValue();

        scheduler.Enqueue(CoroTaskView{simpleWorkCoro.hdl});*/

        scheduler.Enqueue(Test1());

        scheduler.Wait();

        //SyncOut() << "SimpleWorkYieldReturnValue yieldValue: " << simpleWorkCoro.hdl.promise().yieldValue << "\n";
        //SyncOut() << "SimpleWorkYieldReturnValue returnValue: " << simpleWorkCoro.hdl.promise().returnValue << "\n";
    }

    return 0;
}
