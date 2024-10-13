# TINYCORO C++20 Coroutine Library

[![codecov](https://codecov.io/github/kovacsnador/tinycoro/graph/badge.svg?token=WRHPY0TE8D)](https://codecov.io/github/kovacsnador/tinycoro)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)

`tinycoro` is distributed under the MIT license.

## Description
`tinycoro` is a lightweight, header-only coroutine library aimed at simplifying asynchronous programming in C++. It offers an efficient, straightforward solution for managing concurrent tasks, without the complexity of traditional threading or multiprocessing.

This library combines the C++ coroutine API with the familiar promise/future-based concurrency model, making it intuitive and easy to use. It leverages well-known C++ constructs such as `std::promise`, `std::future`, and `std::stop_source`, allowing developers to work with familiar patterns.

## Acknowledgement
I would like to extend my heartfelt thanks to my brother, <a href="https://www.linkedin.com/in/mz-per-x/" target="_blank">`László Kovács`</a>, for his unwavering support and invaluable advice throughout the development of this project. His guidance and encouragement have been a tremendous help. Thank you, Bro! :)

## Motivation
Imagine you have two asynchronous API calls, and one needs to wait for the other to finish. A common example of this scenario might look like the following in traditional C++:

```cpp
void AsyncDownload(const std::string& url, std::function<void(std::string)> callback)
{
    // async callback call...
}

void AsyncPrepareData(const std::string& data, std::function<void(std::string)> callback)
{
    if(data.empty())
    {
        // Maybe throwing an exception here....
        throw std::runtime_error{"Invalid data input."};
    }

    // async callback call...
}
```
In a typical C++ solution, you would use nested (lambda) callbacks, a `std::latch` to manage the waiting and some error handling for possible exceptions.

```cpp
std::string CollectAllDataWithErrorHandling()
{
    std::latch latch{1};
    std::string result;

    std::exception_ptr exceptionPtr{};

    AsyncDownload("http://test.hu", [&latch, &result, &exceptionPtr](std::string data) {
        try
        {
            AsyncPrepareData(data, [&latch, &result] (std::string res) {
                result = std::move(res);
                latch.count_down();
            });
        }
        catch(...)
        {
            // saving the exception to rethrow to the caller.
            exceptionPtr = std::current_exception();
            latch.count_down();
        }
    });

    latch.wait();

    if(exceptionPtr)
    {
        // return the exception
        std::rethrow_exception(exceptionPtr);
    }

    return result;
}
```
While this works, the code quickly becomes messy with nested callbacks exception handling and explicit `thread blocking` synchronization point. This adds unnecessary complexity to what should be a simple sequence of operations.

### `Tinycoro to rescue`

With coroutines and structured concurrency, the code becomes much more readable. There are no nested callbacks, exception handling and no thread blocker waiting points:
```cpp
tinycoro::Task<std::string> CollectAllDataWithErrorHandlingCorouitne()
{
    std::string result;
    co_await tinycoro::AsyncCallbackAwaiter{[](auto cb) { AsyncDownload("http://test.hu", cb); }, [&result](std::string data) { result = std::move(data);}}; 
    co_await tinycoro::AsyncCallbackAwaiter{[&result](auto cb) { AsyncPrepareData(result, cb); }, [&result](std::string res) { result = std::move(res);}};
    co_return result;
}
```
### `Further Simplification with Coroutine Wrappers`

You can make this even more readable by wrapping the asynchronous API calls in their own `tinycoro::Task`. This abstracts away the callback entirely (still with included exception handling):
```cpp
tinycoro::Task<std::string> AsyncDownloadCoro(const std::string& url)
{
    std::string result;
    co_await tinycoro::AsyncCallbackAwaiter{[&url](auto cb) { AsyncDownload(url, cb); }, [&result](std::string data) { result = std::move(data);}};
    co_return result;
}

tinycoro::Task<std::string> AsyncPrepareCoro(std::string data)
{
    co_await tinycoro::AsyncCallbackAwaiter{[&data](auto cb) { AsyncPrepareData(data, cb); }, [&data](std::string res) { data = std::move(res);}};
    co_return data;
}
```
### `Final coroutine Task`
Now, the final coroutine looks even cleaner and more intuitive:
```cpp
tinycoro::Task<std::string> CollectAllDataWithErrorHandlingCorouitne()
{
    auto data = co_await AsyncDownloadCoro("http://test.hu");
    auto result = co_await AsyncPrepareCoro(data); // implicit exception handling here...
    co_return result;
}
```
This approach removes all callback semantics, improves readability and maintainability, turning complex asynchronous workflows into simple, sequential code with the power of coroutines.

### `How to invoke the functions`

The function calls are pretty trivial. It's done with some error handling and that's it.

CollectAllDataWithErrorHandling:
```cpp
try
{
    auto result = CollectAllDataWithErrorHandling();
    std::cout << result << '\n';
}
catch(const std::exception& e)
{
    std::cerr << e.what() << '\n'; // Exception: "Invalid data input."
}
```
CollectAllDataWithErrorHandlingCorouitne:
```cpp
auto future = scheduler.Enqueue(CollectAllDataWithErrorHandlingCorouitne());
try
{
    auto result = tinycoro::GetAll(future);
    std::cout << result << '\n';
}
catch(const std::exception& e)
{
    std::cerr << e.what() << '\n';  // Exception: "Invalid data input."
}
```

## Overview
* [Acknowledgement](#acknowledgement)
* [Motivation](#motivation)
* [Usage](#usage)
* [Examples](#examples)
    - [CoroScheduler](#coroscheduler)
    - [Task](#task)
    - [TaskView](#taskview)
    - [Task with return value](#returnvaluetask)
    - [Task with exception](#exceptiontask)
    - [Nested task](#nestedtask)
    - [Generator](#generator)
    - [Multi Tasks](#multitasks)
    - [AsyncCallbackAwaiter](#asynccallbackawaiter)
    - [AsyncCallbackAwaiter_CStyle](#asynccallbackawaiter_cstyle)
    - [AnyOf](#anyof)
    - [Custom Awaiter](#customawaiter)
    - [SyncAwaiter](#syncawaiter)
    - [AnyOfAwait](#anyofawait)
* [Contributing](#contributing)
* [Support](#support)

## Usage
Simply copy the `include` folder into your C++20 (or greater) project. If necessary, add the include path to your project settings. After that, you can include the library with the following line:

```cpp
#include <tinycoro/tinycoro_all.h>
```

## Examples
The following examples demonstrate various use cases of the `tinycoro` library:

### `CoroScheduler`
The `tinycoro::CoroScheduler` is responsible for managing and executing coroutines across multiple threads.
When creating a scheduler, you can specify the number of worker threads it should use.
The constructor accepts the number of threads, allowing you to utilize hardware concurrency efficiently.

```cpp
#include <tinycoro/tinycoro_all.h>

// create a scheduler
tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};
```

### `Task`
This example demonstrates how to create a basic coroutine task that returns void and schedule it using the tinycoro::CoroScheduler. The scheduler takes complete ownership of the coroutine, managing its lifecycle.

The `Enqueue` function in the `tinycoro::CoroScheduler` is designed to schedule one or more coroutine tasks for execution. It supports both individual and containerized task inputs. The function returns std::future object(s).

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

### `TaskView`
If you don't want to give full ownership of a coroutine to the scheduler, you can use TaskView, which allows you to retain control over the coroutine's lifecycle while still scheduling it for execution.

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

### `ReturnValueTask`
When you schedule a task using the `tinycoro::CoroScheduler`, you can use the `std::future` returned by the scheduler to retrieve the result of the coroutine, just like with any other asynchronous task.

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

### `ExceptionTask`
When scheduling tasks using tinycoro::CoroScheduler, exceptions thrown within a coroutine are propagated through the `std::future`. You can handle these exceptions using the traditional way with the `try-catch` approach when calling `future.get()`.

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

### `NestedTask`
Tinycoro allows you to nest coroutine tasks because a `tinycoro::Task` is an awaitable object. This means that you can co_await on another coroutine task within your task, making it possible to structure tasks hierarchically.

Additionally, exceptions thrown in nested tasks are propagated up to the caller, making error handling straightforward.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_nestedTask(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            // Optionally, you could throw an exception from here
            // throw std::runtime_error("Nested task exception");
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

### `Generator`
Tinycoro provides support for coroutine generators via `tinycoro::Generator`, allowing you to yield values lazily. This can be useful for iterating over sequences of data without needing to create them all at once.

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

### `MultiTasks`
The tinycoro library allows you to enqueue multiple coroutine tasks simultaneously and manage their completion efficiently. The `GetAll` functionality can be used to wait for all enqueued tasks to finish.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiTasks(tinycoro::CoroScheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<void> {
        co_return;
    };

    // enqueue more tasks at the same time.
    auto futures = scheduler.Enqueue(task(), task(), task());
    
    // wait for all complition
    tinycoro::GetAll(futures);
}
```

### `AsyncCallbackAwaiter`
`tinycoro::AsyncCallbackAwaiter` is an awaiter interface that requires an asynchronous callback function or lambda with one parameter, which is the wrapped user callback. The wrapped user callback mimics the same parameters and return value as the original user callback, but it also includes the necessary tools to notify the scheduler to resume the coroutine on the CPU.

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

### `AsyncCallbackAwaiter_CStyle`
Async callback awaiter and usage with a C-style API with return value.

`tinycoro::AsyncCallbackAwaiter_CStyle` is an awaiter interface that requires an asynchronous callback function or lambda with two parameters: the first is the `wrappedCallback`, and the second is the `userData` (usually a void*). The wrapped user callback mimics the same parameters and return value as the original user callback, but it also includes the necessary tools to notify the scheduler to resume the coroutine on the CPU.

This example demonstrates how to use the tinycoro library to handle asynchronous operations that utilize C-style callbacks. The code simulates a third-party asynchronous API that accepts a callback and allows you to await its completion, including the usage of user data passed to the callback.

It highlights the use of `tinycoro::IndexedUserData<0>` to ensure that the correct argument is used in the callback argument list for custom user data. In our case this is the first one `<0>`.

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
            auto d = static_cast<int*>(userData); 
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

### `AnyOf`
This example demonstrates how to use the tinycoro library to run multiple coroutine tasks concurrently and cancel all but the first one that completes. It utilizes tinycoro::AnyOfWithStopSource in combination with a std::stop_source to manage task cancellation effectively.

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

### `CustomAwaiter`
This example demonstrates how to create and use a custom awaiter in conjunction with the tinycoro library. The custom awaiter allows for asynchronous behavior similar to the provided [AsyncCallbackAwaiter](#example17).


```cpp
// Your custom awaiter
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

void Example_CustomAwaiter(auto& scheduler)
{
    auto asyncTask = []() -> tinycoro::Task<int32_t> {
        // do some work before

        auto val = co_await CustomAwaiter{};

        // do some work after
        co_return val;
    };

    auto future = scheduler.Enqueue(asyncTask());
    auto val    = tinycoro::GetAll(future);

    std::cout << "co_return => " << val << '\n'; 
}
```

### `SyncAwaiter`
This example demonstrates how to use the tinycoro library's SyncAwait function to concurrently wait for multiple coroutine tasks to finish and then accumulate their results into a single string.

This example effectively showcases the power of the tinycoro library in managing asynchronous tasks and demonstrates how to efficiently synchronize multiple tasks while accumulating their results in a clean and non-blocking manner.


```cpp
tinycoro::Task<std::string> Example_SyncAwait(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<std::string> { co_return "123"; };
    auto task2 = []() -> tinycoro::Task<std::string> { co_return "456"; };
    auto task3 = []() -> tinycoro::Task<std::string> { co_return "789"; };

    // waiting to finish all other tasks. (non blocking)
    auto tupleResult = co_await tinycoro::SyncAwait(scheduler, task1(), task2(), task3());

    // tuple accumulate
    co_return std::apply(
        []<typename... Ts>(Ts&&... ts) {
            std::string result;
            (result.append(std::forward<Ts>(ts)), ...);
            return result;
        },
        tupleResult);
}
```

### `AnyOfAwait`
This example demonstrates how to use the tinycoro library's `AnyOfAwait` function to concurrently wait for the completion of multiple asynchronous tasks, allowing for non-blocking execution.

Waiting for the first task to complete. Others are cancelled if possible.

```cpp
tinycoro::Task<void> Example_AnyOfCoAwait(auto& scheduler)
{
    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    // Nonblocking wait for other tasks
    auto results = co_await tinycoro::AnyOfAwait(scheduler, task1(1s), task1(2s), task1(3s));

    auto t1 = std::get<0>(results);
    auto t2 = std::get<1>(results);
    auto t3 = std::get<2>(results);
}
```

## Contributing

Contributions to `tinycoro` are welcome and encouraged! If you'd like to contribute, please follow these steps:

1. **Fork the repository**: Start by forking the project on GitHub and cloning your fork locally.
2. **Create a branch**: Create a new branch for your feature or bugfix. Ensure the branch name reflects the purpose of your changes (e.g., `feature/new-feature` or `bugfix/issue-123`).
3. **Make your changes**: Implement your feature or fix. Please ensure your code follows the project's style guidelines and includes appropriate tests.
4. **Commit and push**: Commit your changes with clear, descriptive messages, and push your branch to your forked repository.
5. **Open a pull request**: Submit a pull request (PR) to the main repository. Be sure to explain the purpose and impact of your changes in the PR description.

### Code Style and Guidelines
- Follow the project's existing code structure and style.
- Ensure all code is properly documented, especially new features or APIs.
- Write tests to validate your changes and ensure they do not introduce regressions.

### Reporting Issues
If you find a bug or have a feature request, please open an issue on the GitHub repository. Provide as much detail as possible, including steps to reproduce the issue, expected behavior, and any relevant system information.

We appreciate your contributions to making `tinycoro` even better!

## Support

File bug reports, feature requests and questions using: https://github.com/kovacsnador/tinycoro/issues
