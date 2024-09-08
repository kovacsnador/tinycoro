# TINYCORO C++20 Coroutine Library

`tinycoro` is distributed under the MIT license.

## Description
`tinycoro` is a lightweight, header-only coroutine library aimed at simplifying asynchronous programming in C++. It offers an efficient, straightforward solution for managing concurrent tasks, without the complexity of traditional threading or multiprocessing.

This library combines the C++ coroutine API with the familiar promise/future-based concurrency model, making it intuitive and easy to use. It leverages well-known C++ constructs such as `std::promise`, `std::future`, and `std::stop_source`, allowing developers to work with familiar patterns.

## Overview
* [Usage](#usage)
* [Examples](#examples)
    - [Void Task](#example1)
    - [Void TaskView](#example2)
    - [Task with return value](#example3)
    - [Task return move only](#example4)
    - [Task return aggregate](#example5)
    - [Task with exception](#example6)
    - [Nested task](#example7)
    - [Nested Task with exception](#example8)
    - [Generator](#example9)
    - [Multi Tasks](#example10)
    - [Multi moved dynamic Tasks](#example11)
    - [Multi moved dynamic Tasks with return value](#example12)
    - [Dynamic Tasks](#example13)
    - [Multi Tasks with different return values](#example14)
    - [Sleep example](#example15)
    - [Async pulling](#example16)
    - [AsyncCallbackAwaiter](#example17)
    - [AsyncCallbackAwaiter_CStyle](#example18)
    - [AsyncCallbackAwaiter_CStyle void](#example19)
    - [AsyncCallbackAwaiter with return value](#example20)
    - [Usage with stop token](#example21)
    - [AnyOf](#example22)
    - [AnyOf with dynamic tasks](#example23)
    - [AnyOf with dynamic void tasks](#example24)
    - [AnyOf with exception](#example25)
* [Build Instructions](#build-instructions)
* [Contributing](#contributing)
* [Support](#support)

## Usage
You only need to copy the *include* folder into your C++ 20 (or greater) project. Add the include path in your project settings if needed and after that you can include the library.

```cpp
#include <tinycoro/tinycoro_all.h>
```

## Examples
The following examples demonstrate various use cases of the `tinycoro` library:


### Example1
Simple void Task

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_voidTask()
{
    // create a scheduler
    tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};

    auto task = []() -> tinycoro::Task<void> {
        co_return;
    };

    std::future<void> future = scheduler.Enqueue(task());
    future.get();
}
```

### Example2
Simple void Task as TaskView

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_taskView(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<void> {
        co_return;
    };

    auto coro   = task();
    std::future<void> future = scheduler.Enqueue(coro.TaskView());

    future.get();
}
```

### Example3
Simple task with return value

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_returnValueTask(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        co_return 42;
    };

    std::future<int32_t> future = scheduler.Enqueue(task());
    auto val    = future.get();
}
```

### Example4
Simple task with return value which is only moveable

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_moveOnlyValue(tinycoro::CoroScheduler& scheduler)
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

    auto task = []() -> tinycoro::Task<OnlyMoveable> {
        co_return 42;
    };

    std::future<OnlyMoveable> future = scheduler.Enqueue(task());
    auto val    = future.get();
}
```

### Example5
Aggregate as return value from task

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_aggregateValue(tinycoro::CoroScheduler& scheduler)
{
    struct Aggregate
    {
        int32_t i;
        int32_t j;
    };

    auto task = []() -> tinycoro::Task<Aggregate> {
        co_return Aggregate{42, 43};
    };

    std::future<Aggregate> future = scheduler.Enqueue(task());
    auto val = future.get();
}
```

### Example6
Exception handling

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_exception(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<void> {
        // throw an exception from task
        throw std::runtime_error("Example_exception exception");
        co_return;
    };

    std::future<void> future = scheduler.Enqueue(task());

    try
    {
        // calling get rethrows the exception 
        future.get();
    }
    catch (const std::exception& e)
    {
        // catched the exception
        ...
    }
}
```

### Example7
Nested coroutine task

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_nestedTask(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            co_return 42;
        };

        // calling co_await for nestedTask
        auto val = co_await nestedTask();

        co_return val;
    };

    std::future<int32_t> future = scheduler.Enqueue(task());
    int32_t val42 = future.get();
}
```

### Example8
Nested coroutine task with exception

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_nestedException(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        auto nestedTask = []() -> tinycoro::Task<int32_t> {

            throw std::runtime_error("Example_nestedException nested exception");
            co_return 42;
        };

        auto val = co_await nestedTask();

        co_return val;
    };

    std::future<int32_t> future = scheduler.Enqueue(task());

    try
    {
        auto val = future.get();
    }
    catch (const std::exception& e)
    {
        ...
    }
}
```

### Example9
Generator

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_generator()
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

    for (const auto& it : generator(12))
    {
        it.i;
        ...
    }
}
```

### Example10
Multi tasks with GetAll(futures)

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiTasks(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<void> {
        co_return;
    };

    // enqueue more tasks at the same time.
    auto futures = scheduler.EnqueueTasks(task(), task(), task());
    
    // wait for all complition
    tinycoro::GetAll(futures);
}
```

### Example11
Multi tasks with moved into the scheduler. (return void)

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiMovedTasksDynamicVoid(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = []() -> tinycoro::Task<void> {
        co_return;
    };

    auto task2 = []() -> tinycoro::Task<void> {
        co_return;
    };

    auto task3 = []() -> tinycoro::Task<void> {
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    // std::move tasks vector into the scheduler (scheduler takes over ownership above tasks)
    auto futures = scheduler.EnqueueTasks(std::move(tasks));
    tinycoro::GetAll(futures);
}
```

### Example12
Multi tasks with GetAll(futures) moved into the scheduler. (with task return value)

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiMovedTasksDynamic(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = []() -> tinycoro::Task<int32_t> {
        co_return 41;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        co_return 42;
    };

    auto task3 = []() -> tinycoro::Task<int32_t> {
        co_return 43;
    };

    // creating a vector of tasks dynamically
    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    // std::move tasks vector into the scheduler (scheduler takes over ownership above tasks)
    auto futures = scheduler.EnqueueTasks(std::move(tasks));

    // Wait and collecting all results 
    auto results = tinycoro::GetAll(futures);

    std::cout << "GetAll co_return => " << results[0] << ", " << results[1] << ", " << results[2] << '\n';
}
```

### Example13
Multi tasks dynamic with GetAll(futures). (with task return value)

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiTasksDynamic(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = []() -> tinycoro::Task<int32_t> {
        co_return 41;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {
        co_return 42;
    };

    auto task3 = []() -> tinycoro::Task<int32_t> {
        co_return 43;
    };

    std::vector<tinycoro::Task<int32_t>> tasks;
    tasks.push_back(task1());
    tasks.push_back(task2());
    tasks.push_back(task3());

    // scheduler is using TaskView to refer to the task
    auto futures = scheduler.EnqueueTasks(tasks);
    auto results = tinycoro::GetAll(futures);

    std::cout << "GetAll co_return => " << results[0] << ", " << results[1] << ", " << results[2] << '\n';
}
```

### Example14
Multi tasks with different return values and exception.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiTaskDifferentValues(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = []() -> tinycoro::Task<void> {
        co_return;
    };

    auto task2 = []() -> tinycoro::Task<int32_t> {

        // throwing an exception
        throw std::runtime_error("Exception throwed!");

        co_return 42;
    };

    struct S
    {
        int32_t i;
    };

    auto task3 = []() -> tinycoro::Task<S> {
        co_return 43;
    };

    auto futures = scheduler.EnqueueTasks(task1(), task2(), task3());

    try
    {
        // GetAll return with std::tuple<...>
        auto results = tinycoro::GetAll(futures);

        auto voidType = std::get<0>(results);
        auto val      = std::get<1>(results);
        auto s        = std::get<2>(results);

        std::cout << std::boolalpha << "GetAll task1 co_return => void " << std::is_same_v<tinycoro::VoidType, std::decay_t<decltype(voidType)>>
                  << '\n';
        std::cout << "GetAll task2 co_return => " << val << '\n';
        std::cout << "GetAll task3 co_return => " << s.i << '\n';
    }
    catch (const std::exception& e)
    {
        ...
    }
}
```

### Example15
Sleep task example

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_sleep(tinycoro::CoroScheduler& scheduler)
{
    auto sleep = [](auto duration) -> tinycoro::Task<int32_t> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            // std::suspend_always sending task to end of the queue
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

    std::cout << "co_return => " << future.get() << '\n';
}
```

### Example16
Simple old school async pulling

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_asyncPulling(tinycoro::CoroScheduler& scheduler)
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

        // simple old school pulling (wait for finishing the std::async task)
        while (future.wait_for(0s) != std::future_status::ready)
        {
            co_await std::suspend_always{};
        }

        auto res = future.get();
        co_return res;
    };

    auto task = [&asyncTask]() -> tinycoro::Task<int32_t> {
        auto val = co_await asyncTask(4);
        co_return val;
    };

    std::future<int32_t> future = scheduler.Enqueue(task());
    std::cout << "co_return => " << future.get() << '\n';
}
```

### Example17
Async callback awaiter and usage

```cpp
#include <tinycoro/tinycoro_all.h>

// Simulate a third party async call
void AsyncCallbackAPIvoid(std::regular_invocable<void*, int> auto cb, void* userData)
{
    std::jthread t{[cb, userData] {
        std::this_thread::sleep_for(1s);
        cb(userData, 42);
    }};
    t.detach();
}

void Example_asyncCallbackAwaiter(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        
        // async callback which will be called from third party API
        auto cb = []([[maybe_unused]] void* userData, int i) {
            // do some work
            std::this_thread::sleep_for(100ms);
        };

        // create and co_await for the AsyncCallbackAwaiter
        co_await tinycoro::AsyncCallbackAwaiter([](auto wrappedCallback) { AsyncCallbackAPIvoid(wrappedCallback, nullptr); }, cb);
        co_return 42;
    };

    auto future = scheduler.Enqueue(task());
    std::cout << "co_return => " << future.get() << '\n';
}
```

### Example18
Async callback awaiter and usage with a C-style API with return value

```cpp
#include <tinycoro/tinycoro_all.h>

// Simulate a third party async call
void AsyncCallbackAPIvoid(std::regular_invocable<void*, int> auto cb, void* userData)
{
    std::jthread t{[cb, userData] {
        std::this_thread::sleep_for(1s);
        cb(userData, 42);
    }};
    t.detach();
}

void Example_asyncCallbackAwaiter_CStyle(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        // c free function callback
        auto cb = [](void* userData, int i) {
            
            // Converting back to get your user data
            auto d = tinycoro::UserData::Get<int>(userData); 
            *d = 21;
        };

        auto async = [](auto wrappedCallback, void* wrappedUserData) { AsyncCallbackAPIvoid(wrappedCallback, wrappedUserData); return 21; };
        
        int userData{0};

        auto res = co_await tinycoro::AsyncCallbackAwaiter_CStyle(async, cb, tinycoro::IndexedUserData<0>(&userData));
        
        co_return userData + res;
    };

    auto future = scheduler.Enqueue(task());
    std::cout << "co_return => " << future.get() << '\n';
}
```

### Example19
Async callback awaiter and usage with a C-style API

```cpp
#include <tinycoro/tinycoro_all.h>

// Simulate a third party async call
void AsyncCallbackAPIvoid(std::regular_invocable<void*, int> auto cb, void* userData)
{
    std::jthread t{[cb, userData] {
        std::this_thread::sleep_for(1s);
        cb(userData, 42);
    }};
    t.detach();
}

void Example_asyncCallbackAwaiter_CStyleVoid(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = []() -> tinycoro::Task<void> {
        
        auto task2 = []() -> tinycoro::Task<void> {

            auto cb = [](void* userData, int i) {
                auto null = tinycoro::UserData::Get<std::nullptr_t>(userData);
                assert(null == nullptr);
            };

            std::nullptr_t nullp;

            co_await tinycoro::AsyncCallbackAwaiter_CStyle([](auto cb, auto userData) { AsyncCallbackAPIvoid(cb, userData); }, cb, tinycoro::IndexedUserData<0>(nullp));
        };
        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());
    future.get();
}
```

### Example20
Async callback awaiter with return value

```cpp
#include <tinycoro/tinycoro_all.h>

// Simulate a third party async call
std::jthread AsyncCallbackAPI(void* userData, funcPtr cb, int i = 43)
{
    return std::jthread{[cb, userData, i] {
        std::cout << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(1s);
        cb(userData, 42, i);
    }};
}

void Example_asyncCallbackAwaiterWithReturnValue(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        struct S
        {
            int i{42};
        };

        auto cb = [](void* userData, int i, int j) {

            auto* s = tinycoro::UserData::Get<S>(userData);
            s->i++;

            // do some work
            std::this_thread::sleep_for(100ms);
        };

        S s;

        auto asyncCb = [](auto cb, auto userData){ return AsyncCallbackAPI(userData, cb); };

        // wait with return value
        std::jthread jthread = co_await tinycoro::AsyncCallbackAwaiter_CStyle(asyncCb, cb, tinycoro::IndexedUserData<0>(&s));
        co_return s.i;
    };

    auto future = scheduler.Enqueue(task());
    future.get();
}
```

### Example21
Usage with std::stop_source and std::stop_token

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_usageWithStopToken(tinycoro::CoroScheduler& scheduler)
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

    auto futures = scheduler.EnqueueTasks(task1(1s, source), task2(source.get_token()));

    auto results = tinycoro::GetAll(futures);

    auto task2Val = std::get<1>(results);
    std::cout << "co_return => " << task2Val << '\n';
}
```

### Example22
Simple tasks with inbuild std::stop_source mechanism and AnyOf support.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_AnyOfVoid(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            // This is a cancellable suspend. So the scheduler can cancel the task while is suspended implicitly.
            co_await tinycoro::CancellableSuspend<void>{};
        }
    };

    std::stop_source source;

    // start multiple tasks and wait for the first to complete and cancel all other.
    tinycoro::AnyOfWithStopSource(scheduler, source, task1(1s), task1(2s), task1(3s));
}
```

### Example23
Simple tasks with inbuild std::stop_source mechanism and AnyOf support with return value.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_AnyOf(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    // wait for the first task to complete
    auto results = tinycoro::AnyOf(scheduler, task1(1s), task1(2s), task1(3s));

    auto t1 = std::get<0>(results);
    auto t2 = std::get<1>(results);
    auto t3 = std::get<2>(results);

    std::cout << "co_return => " << t1 << ", " << t2 << ", " << t3 << '\n';
}
```

### Example24
Simple dynamicly generated tasks with inbuild std::stop_source mechanism and AnyOf support with return value.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_AnyOfDynamic(tinycoro::CoroScheduler& scheduler)
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

    std::cout << "co_return => " << results[0] << ", " << results[1] << ", " << results[2] << '\n';
}
```

### Example24
Simple dynamicly generated tasks with inbuild std::stop_source mechanism and AnyOf support. (void tasks)

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_AnyOfDynamicVoid(tinycoro::CoroScheduler& scheduler)
{
    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        
        // you can query the stop_source and stop_token from your coroutine
        [[maybe_unused]] auto stopToken  = co_await tinycoro::StopTokenAwaiter{};
        [[maybe_unused]] auto stopSource = co_await tinycoro::StopSourceAwaiter{};

        // build in sleep for more efficiency and performance
        co_await tinycoro::Sleep(duration);
    };

    std::stop_source source;

    std::vector<tinycoro::Task<void>> tasks;
    tasks.push_back(task1(10ms));
    tasks.push_back(task1(20ms));
    tasks.push_back(task1(30ms));

    tinycoro::AnyOfWithStopSource(scheduler, source, tasks);
}
```

### Example25
Simple tasks with inbuild std::stop_source mechanism and AnyOf support. One of the task throws an exception.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_AnyOfVoidException(tinycoro::CoroScheduler& scheduler)
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
            // throwing an exception (triggers stop_source and cancel all other task)
            throw std::runtime_error("Exception throwed!");
            co_await tinycoro::CancellableSuspend<void>{};
        }
        co_return 42;
    };

    try
    {
        [[maybe_unused]] auto results = tinycoro::AnyOf(scheduler, task1(1s), task1(2s), task1(3s), task2(5s));
    }
    catch (const std::exception& e)
    {
        std::cout << "Exception: " << e.what() << '\n';
    }
}
```