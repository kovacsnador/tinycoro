# TINYCORO C++20 Coroutine Library

[![codecov](https://codecov.io/github/kovacsnador/tinycoro/graph/badge.svg?token=WRHPY0TE8D)](https://codecov.io/github/kovacsnador/tinycoro)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)

`tinycoro` is distributed under the MIT license.

## Join the `TINYCORO` Community
Join us to discuss Tinycoro, ask questions, and share ideas!

[![Join our Discord](https://img.shields.io/badge/Discord-Join-blue?logo=discord&logoColor=white)](https://discord.gg/epJ867PE)

## Description
`tinycoro` is a lightweight, header-only coroutine library aimed at simplifying asynchronous programming in C++. It offers an efficient, straightforward solution for managing concurrent tasks, without the complexity of traditional threading or multiprocessing.

This library combines the C++ coroutine API with the familiar promise/future-based concurrency model, making it intuitive and easy to use. It leverages well-known C++ constructs such as `std::promise`, `std::future`, and `std::stop_source`, allowing developers to work with familiar patterns.

## Acknowledgement
I would like to extend my heartfelt thanks to my brother [`László Kovács`](https://www.linkedin.com/in/mz-per-x/), for his unwavering support and invaluable advice throughout the development of this project. His guidance and encouragement have been a tremendous help. Thank you, Bro! :)

## Overview
* [Acknowledgement](#acknowledgement)
* [Motivation](#motivation)
* [Usage](#usage)
* [Examples](#examples)
    - [Scheduler](#scheduler)
    - [Task](#task)
    - [AllOf](#allof)
    - [AllOfInline](#allofinline)
    - [AnyOf](#anyof)
    - [AnyOfInline](#anyofinline)
    - [InlineTask](#inlinetask)
    - [Cancellation](#cancellation)
    - [MakeBound](#makebound)
    - [Task with return value](#returnvaluetask)
    - [Task with exception](#exceptiontask)
    - [Nested task](#nestedtask)
    - [SoftClock](#softclock)
    - [CancellationToken](#cancellationtoken)
    - [Generator](#generator)
    - [Multi Tasks](#multitasks)
    - [AsyncCallbackAwaiter](#asynccallbackawaiter)
    - [AsyncCallbackAwaiter_CStyle](#asynccallbackawaiter_cstyle)
    - [AnyOf](#anyof)
    - [Custom Awaiter](#customawaiter)
    - [AllOfAwaiter](#syncawaiter)
    - [AnyOfAwait](#anyofawait)
* [Awaitables](#awaitables)
    - [Mutex](#mutex)
    - [Semaphore](#semaphore)
    - [ManualEvent](#manualevent)
    - [AutoEvent](#autoevent)
    - [SingleEvent](#singleevent)
    - [Latch](#latch)
    - [Barrier](#barrier)
    - [BufferedChannel](#bufferedchannel)
    - [UnbufferedChannel](#unbufferedchannel)
* [Allocators](#allocators)
    - [AllocatorAdapter](#allocatoradapter)
    - [Allocator](#allocator)
* [Warning](#warning)
* [Contributing](#contributing)
* [Support](#support)

## Motivation
### Simple breakfast
This example shows how you can use tinycoro to execute multiple asynchronous tasks concurrently—in this case, to prepare breakfast.

### Step 1: Define the Tasks
First, we create two simple coroutine tasks: Toast and Coffee. These simulate the preparation of toast and coffee:
```cpp
tinycoro::Task<std::string> Toast()
{
    // make the toast ready... takes 4 seconds
    std::this_thread::sleep_for(4s);
    
    co_return "toast";
}

tinycoro::Task<std::string> Coffee()
{
    // make the coffee ready... takes 2 seconds
    std::this_thread::sleep_for(2s);

    co_return "coffee";
}
```

### Step 2: Combine the Tasks
Next, we define the Breakfast coroutine, which combines Toast and Coffee preparation. The tinycoro::AllOfAwait function allows both tasks to run concurrently, reducing the total preparation time. Notice the usage of the `co_await` operator here.
```cpp
tinycoro::Task<std::string> Breakfast(tinycoro::Scheduler& scheduler)
{
    // The `AllOfAwait` ensures both `Toast()` and `Caffee()` are executed concurrently.
    auto [toast, coffee] = co_await tinycoro::AllOfAwait(scheduler, Toast(), Caffee());
    co_return *toast + " + " + *coffee;
}

```
### Step 3: Run the Scheduler
Finally, we create a `tinycoro::Scheduler` to manage the execution of the Breakfast coroutine. The total time taken is measured and displayed (4 seconds).
```cpp

    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    auto start = std::chrono::system_clock::now();

    // Start the asynchronous execution of the Breakfast task.
    auto breakfast = tinycoro::AllOf(scheduler, Breakfast(scheduler));

    auto sec = duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start);

    // Breakfast is toast + coffee, Total time 4s
    std::cout << "Breakfast is " << *breakfast << ", Total time " << sec << '\n'; 
```
-------
### Complition callback example
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

With coroutines and structured concurrency, the code becomes much more readable. There are no nested callbacks, exception handling and no thread blocker waiting points: [AsyncCallbackAwaiter](#asynccallbackawaiter)
```cpp
tinycoro::Task<std::string> MyCoroutine()
{
    std::string result;
    co_await tinycoro::AsyncCallbackAwaiter{
        [](auto cb) { AsyncDownload("http://test.hu", cb); },
        [&result](std::string data) { result = std::move(data);}}; 
    
    co_await tinycoro::AsyncCallbackAwaiter{
        [&result](auto cb) { AsyncPrepareData(result, cb); },
        [&result](std::string res) { result = std::move(res);}};
    
    co_return result;
}
```
### `Further Simplification with Coroutine Wrappers`

You can make this even more readable by wrapping the asynchronous API calls in their own `tinycoro::Task`. This abstracts away the callback entirely (still with included exception handling):
```cpp
tinycoro::Task<std::string> AsyncDownloadCoro(const std::string& url)
{
    std::string result;
    co_await tinycoro::AsyncCallbackAwaiter{
        [&url](auto cb) { AsyncDownload(url, cb); },
        [&result](std::string data) { result = std::move(data);}};
    co_return result;
}

tinycoro::Task<std::string> AsyncPrepareCoro(std::string data)
{
    co_await tinycoro::AsyncCallbackAwaiter{
        [&data](auto cb) { AsyncPrepareData(data, cb); },
        [&data](std::string res) { data = std::move(res);}};
    co_return data;
}
```
### `Final coroutine Task`
Now, the final coroutine looks even cleaner and more intuitive:
```cpp
tinycoro::Task<std::string> MyCoroutine()
{
    auto data = co_await AsyncDownloadCoro("http://test.hu");
    auto result = co_await AsyncPrepareCoro(*data); // implicit exception handling here...
    co_return *result;
}
```
This approach removes all callback semantics, improves readability and maintainability, turning complex asynchronous workflows into simple, sequential code with the power of coroutines.

### `How to invoke the coroutine functions`

All you need is a `tinycoro::Scheduler`. In most cases, you'll only need a single instance of the scheduler, but you can use multiple instances if necessary. The Scheduler's `Enqueue(..)` function returns a traditional `std::future`, allowing you to call the `get()` method as you normally would. In this example, the type of the returned future is `std::future<std::optional<std::string>>`.
We use `std::optinal` as the return value, because we need to handle potential task cancellations.
```cpp
try
{
    tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};

    std::future<std::optional<std::string>> future = scheduler.Enqueue(MyCoroutine());
    std::cout << future.get().value() << '\n';
}
catch(const std::exception& e)
{
    std::cerr << e.what() << '\n';  // Exception: "Invalid data input."
}
```

Alternatively, you can use helper functions to aggregate the value or values of multiple futures. For instance, the `tinycoro::AllOf` function simplifies this process:
```cpp
try
{
    auto result = tinycoro::AllOf(scheduler, MyCoroutine());
    std::cout << *result << '\n';
}
catch(const std::exception& e)
{
    std::cerr << e.what() << '\n';  // Exception: "Invalid data input."
}
```

## Usage
Simply copy the `include` folder into your C++20 (or greater) project. If necessary, add the include path to your project settings. After that, you can include the library with the following line:

```cpp
#include <tinycoro/tinycoro_all.h>
```

## Examples
The following examples demonstrate various use cases of the `tinycoro` library:

### `Scheduler`
The `tinycoro::Scheduler` is responsible for managing and executing coroutines across multiple threads.
When creating a scheduler, you can specify the number of worker threads it should use.
The constructor accepts the number of threads, allowing you to utilize hardware concurrency efficiently.

```cpp
#include <tinycoro/tinycoro_all.h>

// create a scheduler with explicit worker thread count
tinycoro::Scheduler scheduler{4}; // 4 worker threads

// Or just use the default constructor which is equivalent to:
// tinycoro::Scheduler scheduler{std::thread::hardware_concurrency()};
tinycoro::Scheduler scheduler; 
```

### `Task`

This example demonstrates how to create a basic coroutine task that returns `void` and schedule it using the `tinycoro::Scheduler`.  
The scheduler takes **complete ownership** of the coroutine and manages its lifecycle.

While you can manually enqueue tasks in `tinycoro::Scheduler`, it is **recommended to use helper functions** like `tinycoro::AllOf(...)` or `tinycoro::AnyOf(...)`.
These functions assign the coroutine to the scheduler, handle cancellation, and return results in a unified, fast and safe way.

The `AllOf(...)` function supports both **individual tasks** and **containers of tasks**. It returns `std::optional<>`, `std::tuple<std::optional<T>...>` or `std::vector<std::optional<T>>` depending on the return type and cancellation state.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_voidTask()
{
    // Create a scheduler
    tinycoro::Scheduler scheduler;

    auto task = []() -> tinycoro::Task<void> {
        co_return;
    };

    // Recommended way to run the task
    tinycoro::AllOf(scheduler, task());
}
```

> 💡 For simplicity, if you want to return `void`, you can also just write `tinycoro::Task<>`.  
> The default template parameter is `void`.

### `co_yield` Support for Task

`tinycoro::Task<T>` also supports the `co_yield` operator, enabling coroutine generators that can yield intermediate values before completing with a final `co_return`.  
The yielded values are retrieved one by one using `co_await`, and the final value is returned as the result of the last `co_await`.

```cpp
auto generator = [](int32_t max) -> tinycoro::Task<int32_t> {
    for (auto it : std::views::iota(0, max))
    {
        co_yield it;
    }
    co_return max;
};

auto consumer = [gen = generator](int32_t max) -> tinycoro::Task<void> {
    int32_t v{};
    auto task = gen(max);
    for (auto it : std::views::iota(0, max))
    {
        v = co_await task; // result from co_yield
        assert(v == it);
    }
    v = co_await task; // Final result from co_return
    assert(v == max);
};

tinycoro::AllOfInline(consumer(42));
```

- co_yield is useful for creating generators and streaming values over time from a coroutine, while still allowing for a final result with co_return.

### Yielding and Returning Different Types

If you want to `co_yield` values of one type and `co_return` a value of another, you can use `std::variant` as the return type of your coroutine.  
This way, all yielded and returned values can be wrapped in a unified type and handled using `std::visit` or `std::holds_alternative`.

```cpp
tinycoro::Task<std::variant<int32_t, bool>> YieldCoroutine()
{
    co_yield 41;     // yields an int32_t
    co_return true;  // returns a bool
}
```
---

### `AllOf`

`tinycoro::AllOf` is one of the **core utilities** in tinycoro. It allows you to run multiple tasks and wait for **all** of them to complete.

You can use it in two modes:

- With a **scheduler** for concurrent execution — using `tinycoro::Task`.
- With **inline cooperative execution** on the current coroutine thread — using either `tinycoro::InlineTask` or `tinycoro::Task`.

> ⚠️ **Important**:
> - Scheduler-based versions (`AllOf`, `AllOfAwait`) **require** `tinycoro::Task`.
> - `tinycoro::InlineTask` **cannot run on a scheduler**.
> - Inline versions (`AllOfInline`, `AllOfInlineAwait`) **can run both** `tinycoro::Task` and `tinycoro::InlineTask`.

---

### With Scheduler (Synchronous)

Runs all given `tinycoro::Task`s using the provided scheduler. Blocks until **all** tasks finish.

```cpp
tinycoro::AllOf(scheduler, task1, task2, ...); // each task is a tinycoro::Task
```

- Executes tasks concurrently via the scheduler.
- **Requires** `tinycoro::Task`.
- Returns when all tasks are complete.

---

### Inline Execution (Synchronous)

Runs all given tasks on the current thread. Blocks until all finish.

```cpp
tinycoro::AllOfInline(task1, task2, ...); // task1, task2 can be Task or InlineTask
```

- Executes cooperatively in the current coroutine context.
- Accepts **both** `tinycoro::Task` and `tinycoro::InlineTask`.
- No scheduler required.

---

### With Scheduler (Coroutine)

Asynchronously waits for all `tinycoro::Task`s to complete via the scheduler.

```cpp
co_await AllOfAwait(scheduler, task1, task2, ...); // each task is a tinycoro::Task
```

- Non-blocking.
- Requires `tinycoro::Task`.
- Runs concurrently using the scheduler.

---

### Inline Execution (Coroutine)

Asynchronously waits for all tasks to complete on the current coroutine thread.

```cpp
co_await AllOfInlineAwait(task1, task2, ...); // task1, task2 can be Task or InlineTask
```

- Non-blocking.
- Runs cooperatively.
- Works with both `Task` and `InlineTask`.

---

🔹 All variants return `std::optional`, `std::tuple<std::optional<...>>`, or `std::vector<std::optional<...>>` — always preserving order.

💡 Use:
- `tinycoro::Task` with scheduler-based functions.
- `tinycoro::InlineTask` or `tinycoro::Task` with inline functions.

---

### `AnyOf`

`tinycoro::AnyOf` waits until **any** of the given tasks finishes — ideal for first-available-result scenarios or racing multiple operations.

Same pattern applies here:
- Scheduler-based versions use `tinycoro::Task`.
- Inline versions support both `Task` and `InlineTask`.

---

### With Scheduler (Synchronous)

Runs all `tinycoro::Task`s via the scheduler. Blocks until **any one** finishes.

```cpp
tinycoro::AnyOf(scheduler, task1, task2, ...); // each task is a tinycoro::Task
```

- Executes concurrently.
- Requires `tinycoro::Task`.
- Returns on first completion.

---

### Inline Execution (Synchronous)

Runs all tasks cooperatively on the current thread.

```cpp
tinycoro::AnyOfInline(task1, task2, ...); // task1, task2 can be Task or InlineTask
```

- No scheduler needed.
- Accepts both `tinycoro::Task` and `tinycoro::InlineTask`.
- Returns after the first task completes.

---

### With Stop Source (Synchronous)

Supports early cancellation of other tasks once the first finishes.

```cpp
tinycoro::AnyOfInline(stopSource, task1, task2, ...);
tinycoro::AnyOf(scheduler, stopSource, task1, task2, ...);
```

- Cancels other tasks when one completes.
- StopSource works in both modes.

---

### With Scheduler (Coroutine)

Asynchronously races `tinycoro::Task`s via the scheduler.

```cpp
co_await AnyOfAwait(scheduler, task1, task2, ...);
co_await AnyOfAwait(scheduler, stopSource, task1, task2, ...);
```

- Non-blocking.
- Requires `tinycoro::Task`.
- First task to complete wins.

---

### Inline Execution (Coroutine)

Asynchronously races all tasks in the current coroutine context.

```cpp
co_await AnyOfInlineAwait(task1, task2, ...);
co_await AnyOfInlineAwait(stopSource, task1, task2, ...);
```

- Cooperative, no scheduler.
- Accepts both `tinycoro::Task` and `tinycoro::InlineTask`.

---

🔹 `AnyOf` and `AnyOfAwait` return a `std::tuple<std::optional<...>>` or `std::vector<std::optional<...>>` that identifies which task completed first.

💡 Summary:
- Use `tinycoro::Task` with `AllOf/AnyOf` (scheduler-based).
- Use `tinycoro::Task` or `tinycoro::InlineTask` with `AllOfInline/AnyOfInline`.

















### `AllOf`

`tinycoro::AllOf` is one of the **core utilities** in tinycoro. It allows you to run multiple tasks and wait for **all** of them to complete.

You can use it either with a scheduler for **concurrent execution**, or without a scheduler for **inline execution** on the current thread. You can also `co_await` the coroutine-friendly variant `AllOfAwait(...)` to wait asynchronously.

---

### With Scheduler (Synchronous)

Runs all given tasks using the provided scheduler. Blocks until **all** tasks finish.

```cpp
tinycoro::AllOf(scheduler, task1, task2, ...);
```
---
- Executes tasks concurrently via the scheduler.
- Returns when all tasks are complete.
- Ideal for multi-threaded environments or task scheduling.

### Without Scheduler (Synchronous)
Runs all given tasks on the current thread. Blocks until all tasks finish.
```cpp
tinycoro::AllOf(task1, task2, ...);
```
- Executes tasks concurrently in the current coroutine thread context.
- Useful in single-threaded environments or when no scheduler is needed.

###  With Scheduler (Coroutine)
Use co_await to wait for all tasks asynchronously, executed via the scheduler.

```cpp
co_await AllOfAwait(scheduler, task1, task2, ...);
```
- Non-blocking — returns control to the caller coroutine.
- Schedules tasks concurrently and resumes when all are complete.
- Suitable for composing async flows in a coroutine-friendly way.

###  Without Scheduler (Coroutine)
Use co_await to wait for all tasks inline, in the current coroutine execution context.

```cpp
co_await AllOfAwait(task1, task2, ...);
```
- Non-blocking — stays on the current thread/coroutine.
- Runs tasks cooperatively, resuming once all finish.
----

🔹 Both tinycoro::AllOf(...) and co_await AllOfAwait(...) return `std::optional` or `std::tuple<std::optional>` or `std::vector<std::optional>` of results from the tasks, preserving the order.

💡 Use the version that best fits your concurrency model: synchronous or coroutine-based, with or without a scheduler.

###  `AnyOf`

`tinycoro::AnyOf` is another core utility in tinycoro. It allows you to wait until **any** of the given tasks finishes — useful when you want the **first available result** or need to implement early cancellation.

You can use it with or without a scheduler, and you can `co_await` the coroutine-friendly version `AnyOfAwait(...)`.

---

### With Scheduler

Runs all given tasks using the provided scheduler. Blocks until **any one** of them finishes.

```cpp
tinycoro::AnyOf(scheduler, task1, task2, ...);
```
- Executes tasks concurrently using the scheduler.
- Returns as soon as the first task finishes.
- Useful for racing multiple operations.

### Without Scheduler (Synchronous)
Runs all tasks on the current thread and blocks until the first finishes.

```cpp
tinycoro::AnyOf(task1, task2, ...);
```
- Executes tasks sconcurrently on the current thread.
- Returns after the first completed result.
- Lightweight and scheduler-free.

### With Stop Source (Synchronous)
Supports cancellation for the losing tasks once the first task completes.

```cpp
tinycoro::AnyOf(stopSource, task1, task2, ...);
tinycoro::AnyOf(scheduler, stopSource, task1, task2, ...);
```
- When one task finishes, the others receive a stop request.
- Prevents wasteful work.
### With Scheduler (Coroutine)
Use co_await to wait asynchronously until one task finishes, scheduled via the scheduler.

```cpp
co_await AnyOfAwait(scheduler, task1, task2, ...);
co_await AnyOfAwait(scheduler, stopSource, task1, task2, ...);
```
- Non-blocking.
- Runs all tasks concurrently using the scheduler.
- Resumes as soon as one task finishes.
- Cancels other tasks if stopSource is provided.
### Without Scheduler (Coroutine)
Use co_await to wait inline until the first task completes.

```cpp
co_await AnyOfAwait(task1, task2, ...);
co_await AnyOfAwait(stopSource, task1, task2, ...);
```
- Non-blocking.
- Runs tasks cooperatively in the current coroutine context.
- Resumes after the first result is ready.
---
🔹 `AnyOf` and `AnyOfAwait` return a `std::tuple<std::optional>` or `std::vector<std::optional>` that identifies which task finished, depending on your configuration.

💡 Use stopSource if you want early cancellation of the other tasks to save resources.

## `InlineTask`

`tinycoro::InlineTask<T>` is a **lightweight coroutine type** with the same cancellation support as `Task<T>`, but it differs in several important ways:

- It does **not** interact with the `tinycoro::Scheduler`.
- It always runs **on the current thread**, no asynchronous execution.
- It uses **significantly less memory** than `Task`, since it doesn't need to store any scheduling state.

This makes `InlineTask` ideal for **local, synchronous coroutine flows** where you don't need scheduling or cross-thread execution.

You can `co_await` an `InlineTask` directly from within another coroutine, or run it synchronously using the `tinycoro::AllOf(...)` or `tinycoro::AnyOf(...)` helper function.

```cpp
#include <tinycoro/tinycoro_all.h>

tinycoro::InlineTask<int> InlineTask(int val)
{
    co_return val;
}

tinycoro::Task<int> Task(int val)
{
    // Option 1: co_await inside another coroutine 
    auto value = co_await InlineTask(val);

    co_return value;
}

void RunExample()
{
    // Option 2: run synchronously
    //
    // You don't need to be inside a coroutine context
    // to use AllOf — it works in any regular function too.
tinycoro::AllOf(scheduler, InlineTask(1), InlineTask(2)); // Error
    auto [val_41, val_42] = tinycoro::AllOf(InlineTask(41), InlineTask(42));
}
```

⚠️ Important Limitation
❌ InlineTask cannot be used with the scheduler-based variants of functions like AllOf, AnyOf, or any other function that expects tasks to be scheduled on a tinycoro::Scheduler.

```cpp
// ❌ This will NOT work:
tinycoro::AllOf(scheduler, InlineTask(1), InlineTask(2)); // Error

// ✅ Use this instead if you want synchronous execution:
tinycoro::AllOf(InlineTask(1), InlineTask(2)); // OK
```

### When to use `InlineTask`

Use `InlineTask` when:
- You **don't need scheduler-based execution**.
- You want to keep coroutine execution **strictly on the current thread**.
- You want to build small, composable coroutine helpers.
- You **still want cancellation support**, but without the cost of task queuing or thread management.

Use `Task` when:
- You need **asynchronous coroutine execution**.

### `Cancellation`

By default, all `Task` and `InlineTask` instances are **cancellable at their initial suspend point**.  
This allows cancellation to prevent a coroutine from starting execution at all — useful for avoiding unnecessary work.

If you want to **disable cancellation at the initial suspend**, you can use the `tinycoro::noninitial_cancellable` policy as the 3. template parameter:

```cpp
tinycoro::Task<void, tinycoro::DefaultAllocator, tinycoro::noninitial_cancellable>
tinycoro::InlineTask<void, tinycoro::DefaultAllocator, tinycoro::noninitial_cancellable>
```

But much more conveniently, you can just use the provided type aliases, which are recommended and super easy to use:
```cpp
tinycoro::TaskNIC<void>
tinycoro::InlineTaskNIC<void>

// NIC stands for 'non initial cancellable'.
```
These aliases make your code cleaner and easier to read—use them whenever possible.

All other suspension points are **not** cancellable by default, but you can explicitly make them cancellable by wrapping the awaitable in `tinycoro::Cancellable`, like this:
```cpp
co_await tinycoro::Cancellable{autoEvent.Wait()};
```
This works with almost all awaitables in tinycoro, including `Task` and `InlineTask` themselves (since they are awaitables too).
It gives you fine-grained control over cancellation at any suspension point.

> ℹ️ You can also **read cancellation behavior directly from the code**:  
> Every cancellable suspension point is explicitly wrapped with `tinycoro::Cancellable{...}` — making it easy to see where cancellation may occur.


### `MakeBound`
If you want to manage the lifetime of a coroutine function and its associated task together, you can use the `tinycoro::MakeBound` factory function. This function creates a `tinycoro::Task<>`, which encapsulates the coroutine function. This ensures that the task cannot outlive it's coroutine function, avoiding common pitfalls associated with coroutines and lambda expressions.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_MakeBound()
{
    int value;

    // Creating the task with MakeBound()
    auto task = tinycoro::MakeBound([&]() -> tinycoro::Task<void> {
        value++;
        co_return;
    });

    tinycoro::AllOf(scheduler, std::move(task))
}
```

#### ⚠️General recomendation
Use statefull lambda functions (lambdas with capture block) with caution in a coroutine environment. They can cause lifetime issues. A better approach is to pass the necessary dependencies explicitly through function parameters, like so. `[](auto& dep1, auto& dep2... ) -> tinycoro::Task<void> {...}; `  

```cpp
#include <tinycoro/tinycoro_all.h>

struct MyClass
{
    int32_t m_value{};

    auto MemberFunction(tinycoro::Scheduler& scheduler)
    {
        auto coro = [this]() -> tinycoro::Task<int32_t> {
            co_return ++m_value;
        };

        // We are not waiting for the Task, so coro is destroyed after function return.
        // To make it safe we need to use tinycoro::MakeBound
        return scheduler.Enqueue(tinycoro::MakeBound(coro));
    }
};
```

### `ReturnValueTask`
When you schedule a task using the `tinycoro::Scheduler`, you can use the `std::future` returned by the scheduler to retrieve the result of the coroutine, just like with any other asynchronous task.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_returnValueTask(tinycoro::Scheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        co_return 42;
    };

    std::optional<int32_t> val42 = tinycoro::AllOf(scheduler, task());
}
```

### `ExceptionTask`
When scheduling tasks using tinycoro::Scheduler, exceptions thrown within a coroutine are propagated through the `std::future`. You can handle these exceptions using the traditional way with the `try-catch` approach when calling `future.get()`.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_exception(tinycoro::Scheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<void> {
        // throw an exception from task
        throw std::runtime_error("Example_exception exception");
        co_return;
    };

    try
    {
        // calling AllOf throws the exception
        tinycoro::AllOf(scheduler, task());
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

void Example_nestedTask(tinycoro::Scheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        auto nestedTask = []() -> tinycoro::Task<int32_t> {
            // Optionally, you could throw an exception from here
            // throw std::runtime_error("Nested task exception");
            co_return 42;
        };

        // calling co_await for nestedTask
        auto val = co_await nestedTask();

        co_return *val;
    };

    auto val42 = tinycoro::AllOf(scheduler, task());
}
```

### `SoftClock`
The `tinycoro::SoftClock` class is a lightweight, thread-safe timer that allows you to register callbacks to be executed after a specified duration or at a specific time point.

#### **Key Features**
- **Thread Safety**: Protected by a mutex and uses `std::condition_variable_any` for event notification.
- **Cancellation Support**: Events can be canceled using a `CancellationToken` or `std::stop_token`.
- **Custom Frequency**: Allows setting a custom update frequency for the clock (minimum frequency is 40ms).

#### **Constructors**
- **`SoftClock(Frequency frequency = 100ms)`**  
  Constructs a `SoftClock` with a custom update frequency.  
  - `frequency`: The frequency at which the clock checks for timed-out events (default is 100ms). The minimum allowed frequency is 40ms.

- **`SoftClock(std::stop_token stopToken, Frequency frequency = 100ms)`**  
  Constructs a `SoftClock` with a custom update frequency and a stop token.  
  - `stopToken`: A `std::stop_token` to allow external control over the clock's execution.  
  - `frequency`: The frequency at which the clock checks for timed-out events (default is 100ms). The minimum allowed frequency is 40ms.

#### **Public Methods**
- **`Register(CbT&& cb, Duration duration)`**  
  Registers a callback to be executed after the specified duration.  
  - `cb`: Callback to execute (must be nothrow-invocable).  
  - `duration`: Time duration after which the callback will be executed.

- **`Register(CbT&& cb, TimePoint timePoint)`**  
  Registers a callback to be executed at the specified time point.  
  - `cb`: Callback to execute (must be nothrow-invocable).  
  - `timePoint`: Time point at which the callback will be executed.

- **`RegisterWithCancellation(CbT&& cb, Duration duration)`**  
  Registers a callback and returns a `CancellationToken` that can be used to cancel the event.  
  - `cb`: Callback to execute (must be nothrow-invocable).  
  - `duration`: Time duration after which the callback will be executed.  
  - Returns: A `CancellationToken` for canceling the event.

- **`RegisterWithCancellation(CbT&& cb, TimePoint timePoint)`**  
  Registers a callback and returns a `CancellationToken` that can be used to cancel the event.  
  - `cb`: Callback to execute (must be nothrow-invocable).  
  - `timePoint`: Time point at which the callback will be executed.  
  - Returns: A `CancellationToken` for canceling the event.

- **`RequestStop()`**  
  Requests the `SoftClock` to stop processing events. This will stop the internal worker thread.

- **`Frequency()`**  
  Returns the current update frequency of the `SoftClock`.

- **`StopRequested()`**  
  Returns `true` if a stop has been requested for the `SoftClock`.

- **`Now()`**  
  Returns the current time point using `std::chrono::steady_clock`.

### `CancellationToken`

The `tinycoro::CancellationToken` class provides a mechanism to cancel registered events in the `SoftClock`. It is returned by the `RegisterWithCancellation` methods of the `SoftClock` class.

### **Key Features**
- **Move-Only**: Supports move semantics but cannot be copied.
- **Automatic Cancellation**: Automatically cancels the event when the token is destroyed.
- **Manual Cancellation**: Allows explicit cancellation of the event.

### **Public Methods**
- **`Release()`**  
  Detaches the token from its parent `SoftClock` without canceling the event.

- **`TryCancel()`**  
  Attempts to cancel the event. Returns `true` if the event was successfully canceled, `false` otherwise (most likely the event was already fired...).



```cpp
int main() {
    tinycoro::SoftClock clock;

    // Register a callback to execute after 1 second
    clock.Register([] { std::cout << "1 second passed!\n"; }, 1s);

    // Register a callback with cancellation support
    auto token = clock.RegisterWithCancellation([] { std::cout << "2 seconds passed!\n"; }, 2s);

    // Cancel the second event
    if (token.TryCancel()) {
        std::cout << "Event canceled!\n";
    }

    // Wait for events to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));

    return 0;
}
```
The same approuch is used to handle sleeps in a coroutine environment.
- **`tinycoro::SleepFor`**
  - The sleep can be interrupted, but the coroutine **will resume** after the interruption.
- **`tinycoro::SleepUntil`**
  - The sleep can be interrupted, but the coroutine **will resume** after the interruption.
- **`tinycoro::SleepForCancellable`**
  - The sleep can be interrupted, and the coroutine **will NOT resume** after the interruption.
- **`tinycoro::SleepUntilCancellable`**
  - The sleep can be interrupted, and the coroutine **will NOT resume** after the interruption.

```cpp
#include <tinycoro/tinycoro_all.h>

tinycoro::SoftClock clock;

tinycoro::Task<void> Task()
{
    // do something

    // this is a coroutine friendly sleep
    co_await tinycoro::SleepFor(clock, 2s);

    // after 2 seconds we get back the control...
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
The tinycoro library allows you to enqueue multiple coroutine tasks simultaneously and manage their completion efficiently. The `AllOf` functionality can be used to wait for all enqueued tasks to finish.

in this example `AllOf` returns a `std::tuple<>` which contains `std::optional` objects.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_multiTasks(tinycoro::Scheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        co_return 42;
    };
    
    // wait for all complition
    auto [result1, result2, result3] = tinycoro::AllOf(scheduler, task(), task(), task());
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

void Example_asyncCallbackAwaiter(tinycoro::Scheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {
        
        // async callback which will be called from third party API
        auto cb = []([[maybe_unused]] void* userData, int i) {
            // do some work
            std::this_thread::sleep_for(100ms);
        };

        // create and co_await for the AsyncCallbackAwaiter
        co_await tinycoro::AsyncCallbackAwaiter(
            [](auto wrappedCallback) { AsyncCallbackAPIvoid(wrappedCallback, nullptr); }, cb);
        co_return 42;
    };

    auto val = tinycoro::AllOf(scheduler, task());
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

void Example_asyncCallbackAwaiter_CStyle(tinycoro::Scheduler& scheduler)
{
    auto task = []() -> tinycoro::Task<int32_t> {

        // c free function callback
        auto cb = [](void* userData, int i) {
            
            // Converting back to get your user data
            auto d = static_cast<int*>(userData); 
            *d = 21;
        };

        auto async = [](auto wrappedCallback, void* wrappedUserData) { 
            AsyncCallbackAPIvoid(wrappedCallback, wrappedUserData); return 21; };
        
        int userData{0};

        auto res = co_await tinycoro::AsyncCallbackAwaiter_CStyle(
            async, cb, tinycoro::IndexedUserData<0>(&userData));
        
        co_return userData + res;
    };

    auto val = tinycoro::AllOf(scheduler, task());
}
```

### `AnyOf`
This example demonstrates how to use the tinycoro library to run multiple coroutine tasks concurrently and cancel all but the first one that completes. It utilizes tinycoro::AnyOf in combination with a std::stop_source to manage task cancellation effectively.

```cpp
#include <tinycoro/tinycoro_all.h>

void Example_AnyOfVoid(tinycoro::Scheduler& scheduler)
{
    auto task1 = [](auto duration) -> tinycoro::Task<void> {
        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            // This is a cancellable suspend. So the scheduler can cancel the task while is suspended implicitly.
            co_await tinycoro::CancellableSuspend{};
        }
    };

    std::stop_source source;

    // start multiple tasks and wait for the first to complete and cancel all other.
    tinycoro::AnyOf(scheduler, source, task1(1s), task1(2s), task1(3s));
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
        _resumeTask = tinycoro::context::PauseTask(hdl);

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

void Example_CustomAwaiter(tinycoro::Scheduler& scheduler)
{
    auto asyncTask = []() -> tinycoro::Task<int32_t> {
        // do some work before

        auto val = co_await CustomAwaiter{};

        // do some work after
        co_return val;
    };

    auto val = tinycoro::AllOf(scheduler, asyncTask()); 
}
```

### `AllOfAwaiter`
This example demonstrates how to use the tinycoro library's AllOfAwait function to concurrently wait for multiple coroutine tasks to finish and then accumulate their results into a single string.

This example effectively showcases the power of the tinycoro library in managing asynchronous tasks and demonstrates how to efficiently synchronize multiple tasks while accumulating their results in a clean and non-blocking manner.


```cpp
tinycoro::Task<std::string> Example_AllOfAwait(tinycoro::Scheduler& scheduler)
{
    auto task1 = []() -> tinycoro::Task<std::string> { co_return "123"; };
    auto task2 = []() -> tinycoro::Task<std::string> { co_return "456"; };
    auto task3 = []() -> tinycoro::Task<std::string> { co_return "789"; };

    // waiting to finish all other tasks. (non blocking)
    auto tupleResult = co_await tinycoro::AllOfAwait(scheduler, task1(), task2(), task3());

    // tuple accumulate
    co_return std::apply(
        []<typename... Ts>(Ts&&... ts) {
            std::string result;
            (result.append(*ts), ...);
            return result;
        },
        tupleResult);
}
```

### `AnyOfAwait`
This example demonstrates how to use the tinycoro library's `AnyOfAwait` function to concurrently wait for the completion of multiple asynchronous tasks, allowing for non-blocking execution.

Waiting for the first task to complete. Others are cancelled if possible.

```cpp
tinycoro::Task<void> Example_AnyOfCoAwait(tinycoro::Scheduler& scheduler)
{
    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{};
        }
        co_return count;
    };

    // Nonblocking wait for other tasks
    auto [t1, t2, t3] = co_await tinycoro::AnyOfAwaitAsync(scheduler, task1(1s), task1(2s), task1(3s));
}
```

## Awaitables

### `Mutex`

The `tinycoro::Mutex` class provides an efficient coroutine-compatible mutual exclusion mechanism. It ensures that only one coroutine can access a critical section at a time, simplifying synchronization in coroutine-based systems. 

The `co_await` operator returns a `tinycoro::ReleaseGuard` object, which utilizes its destructor (RAII) to automatically release the lock

```cpp
    void Example_Mutex(tinycoro::Scheduler& scheduler)
    {
        tinycoro::Mutex mutex;

        int32_t count{0};

        auto task = [&]() -> tinycoro::Task<int32_t> {
            auto lock = co_await mutex;
            co_return ++count;
        };

        auto [c1, c2, c3] = tinycoro::AllOf(scheduler, task(), task(), task());

        // Every varaible should have unique value (on intel processor for sure :) ).
        // So (c1 != c2 && c2 != c3 && c3 != c1)
        // possible output: c1 == 1, c2 == 2, c3 == 3
    }
```

### `Semaphore`

The tinycoro::Semaphore is a counting semaphore designed for controlling concurrent access to shared resources in a coroutine-friendly manner. It ensures that only a limited number of tasks can acquire the semaphore at any given time, and the remaining tasks will be suspended until the semaphore becomes available.

In the example below, a semaphore with an initial count of 1 is created, ensuring that only one task can access the shared resource (in this case, incrementing a counter) at a time. The example demonstrates the use of a semaphore to synchronize multiple coroutines, where each task acquires the semaphore, increments the counter, and then releases it.

The `co_await` operator returns a `tinycoro::ReleaseGuard` object, which utilizes its destructor (RAII) to automatically release the lock

```cpp
    void Example_Semaphore(tinycoro::Scheduler& scheduler)
    {
        tinycoro::Semaphore semaphore{1};

        int32_t count{0};

        auto task = [&semaphore, &count]() -> tinycoro::Task<int32_t> {
            auto lock = co_await semaphore;
            co_return ++count;
        };

        auto [ c1, c2, c3] = tinycoro::AllOf(scheduler, task(), task(), task());

        // Every varaible should have unique value (on intel processor for sure :) ).
        // So (c1 != c2 && c2 != c3 && c3 != c1)
        // possible output: c1 == 1, c2 == 2, c3 == 3
    }
```

In this case, the semaphore ensures that even though the tasks are running concurrently, only one can increment the counter at a time. As a result, c1, c2, and c3 will have unique values, verifying that the synchronization mechanism works as expected.

### `ManualEvent`

The `tinycoro::ManualEvent` is a synchronization primitive in tinycoro that supports multiple consumers waiting on a single event. Unlike `SingleEvent`, `ManualEvent` can be set to allow multiple coroutines to continue concurrently and provides a manual reset capability, making it suitable for cases where multiple awaiters need to be notified of an update.
```cpp
tinycoro::ManualEvent event;
std::string value;


tinycoro::Task<void> Consumer()
{
    co_await event;

    // value is updated, we can use it
    DoSomethingWithValue(value);
}

void Producer()
{
    value = GetValue();

    // trigger all awaiters, the value is there...
    event.Set();
}
```
### `AutoEvent`

The `tinycoro::AutoEvent` is a synchronization primitive in tinycoro that supports multiple consumers waiting on a single event but releases only one awaiter at a time. It features an automatic reset mechanism: once an awaiter is released, the event automatically resets, requiring a new `Set()` call to release the next awaiter.
```cpp
struct MyStruct
{
    void increment()
    {
        count++;
    }

    size_t count{};
};

tinycoro::AutoEvent event;
std::unique_ptr<MyStruct> ptr;
 
tinycoro::Task<void> Consumer()
{
    // waiting for event.
    co_await event;

    // This is thread safe, only 1 consumer can increase the count at the time.
    ptr->increment();

    // trigger the event for the next waiter.
    event.Set();
}

void AllocateAndSet()
{
    // allocate for ptr
    ptr = std::make_unique<MyStruct>();

    // set the event, struct is allocated 
    event.Set();
}

void Run(auto& scheduler)
{
    AllocateAndSet();

    tinycoro::AllOf(scheduler, Consumer(), Consumer(), Consumer());

    // After finishing With 3 consumers at the same time, the ptr->count should be also 3
    assert(ptr->count == 3);
}
```
### `SingleEvent`

The `tinycoro::SingleEvent<T>` is a coroutine-friendly synchronization primitive within tinycoro designed for signaling events between a producer and consumer in an asynchronous environment. The consumer coroutine awaits an event until the producer sets a specific value, which is then retrieved by the consumer. The event can be set only once and allows only one consumer.

```cpp
tinycoro::SingleEvent<int32_t> singleEvent;

tinycoro::Task<void> Consumer()
{
    // waiting for the value to be set
    auto value = co_await singleEvent;

    // value is 42
    assert(value == 42);
}

void Producer()
{
    // setting the value
    singleEvent.SetValue(42);
}
```

### `Latch`

The `tinycoro::Latch` is a countdown-based synchronization primitive in tinycoro. It allows one or more consumers to wait until the latch count reaches zero, at which point all awaiting coroutines are released. This is ideal for coordinating tasks where multiple steps or actions must complete before proceeding.
```cpp
tinycoro::Latch latch{8};

tinycoro::Task<void> Waiter()
{
    // waiting for the latch to be released
    co_await latch;

    //Latch is done do something...
};

void Producer()
{
    // do some work here...
    
    // count down latch.
    latch.CountDown();
};
```

### `Barrier`

`tinycoro::Barrier` is a robust synchronization primitive designed to coordinate a group of coroutines, ensuring they all reach a common synchronization point before proceeding. It supports phased synchronization, auto-resets, and provides the flexibility of a completion handler, which can be invoked when all participants have arrived. This enables efficient multi-stage coordination across asynchronous tasks.

You can use the built-in awaiters, such as `Wait()`, `ArriveAndWait()`, and `ArriveWaitAndDrop()`, to achieve flexible and efficient synchronization.
```cpp

auto onCompletion = []{ std::cout << "phrase completed\n"; };

tinycoro::Barrier barrier{8, onCompletion};

tinycoro::Task<void> Waiter()
{
    // decrements the count and waits for the next phrase
    co_await barrier.ArriveAndWait();

    // do something...

    // again, decrements the count and waits for the next phrase
    co_await barrier.ArriveAndWait();

    // do something...
};
```

### `BufferedChannel`

The `tinycoro::BufferedChannel<T>` is an asynchronous communication primitive in tinycoro designed for passing messages between multiple coroutines. It supports a queue that allows producers to push values into the channel, and consumers can retrieve these values in a coroutine-friendly way (MPMC). The channel can also be closed to signal that no more items will be produced. See also [EChannelOpStatus](#echannelopstatus)

#### Key Features

1. **Coroutines Support**: The channel integrates seamlessly with coroutines, enabling efficient asynchronous data transfer.
2. **Configurable Buffer Size**: The channel's buffer size can be specified during construction. The default size is the maximum value of `size_t`.
3. **Custom Awaiters**: The behavior of popping, pushing, and listening can be customized through template parameters.
4. **Thread Safety**: The channel operations are thread-safe, ensuring reliable communication across multiple threads.
5. **Graceful Shutdown**: Supports orderly channel closure while notifying all waiting coroutines.

#### Constructor

```
BufferedChannel(size_t maxQueueSize = std::numeric_limits<size_t>::max(), std::function<void(ValueT&)> cleanupFunc = {});

BufferedChannel(std::function<void(ValueT&)> cleanupFunc);
```
- **`maxQueueSize`**: The maximum number of elements the channel can buffer. Must be greater than zero. Default value is `std::numeric_limits<size_t>::max()`

- **`cleanupFunc`**: This is a optional cleanup function for the elements which are stucked in the channel after close was performed.

#### Public Methods

- **PopWait**: Awaits the next value from the channel. (with `co_await`)
  ```cpp
  [[nodiscard]] auto PopWait(ValueT& val);
  ```
  - **`val`**: The variable to store the popped value.

- **WaitForListeners**: Waits until the specified number of listeners are present. (with `co_await`)
  ```cpp
  [[nodiscard]] auto WaitForListeners(size_t listenerCount);
  ```

- **PushWait**: Awaits until a value can be pushed into the channel. (with `co_await`)
  ```cpp
  template <typename... Args>
  [[nodiscard]] auto PushWait(Args&&... args);
  ```

- **PushAndCloseWait**: Pushes a value and closes the channel. (with `co_await`)
  ```cpp
  template <typename... Args>
  [[nodiscard]] auto PushAndCloseWait(Args&&... args);
  ```

- **Push**: Blocks the calling thread until the value is pushed into the channel. (NOT awaitable)
  ```cpp
  template <typename... Args>
  void Push(Args&&... args);
  ```

- **PushAndClose**: Pushes a value into the channel and closes it. (NOT awaitable)
  ```cpp
  template <typename... Args>
  void PushAndClose(Args&&... args);
  ```

- **TryPush**: Attempts to push a value into the channel without blocking.
  ```cpp
  template <typename... Args>
  bool TryPush(Args&&... args);
  ```

- **TryPushAndClose**: Attempts to push a value into the channel and close it, without blocking.
  ```cpp
  template <typename... Args>
  bool TryPushAndClose(Args&&... args);
  ```

- **Empty**: Checks if the channel is empty.
  ```cpp
  [[nodiscard]] bool Empty() const noexcept;
  ```

- **Size**: Gets the current number of elements in the channel.
  ```cpp
  [[nodiscard]] auto Size() const noexcept;
  ```

- **Close**: Closes the channel, notifying all awaiters.
  ```cpp
  void Close();
  ```

- **IsOpen**: Checks if the channel is open.
  ```cpp
  [[nodiscard]] bool IsOpen() const noexcept;
  ```

- **MaxSize**: Returns the maximum size of the channel.
  ```cpp
  [[nodiscard]] auto MaxSize() const noexcept;
  ```

#### Exceptions

- **`BufferedChannelException`**: Thrown if invalid operations are attempted, such as pushing to a closed channel or initializing the channel with a zero buffer size.

#### Example:
```cpp
tinycoro::BufferedChannel<int32_t> channel;

tinycoro::Task<void> Consumer()
{
    int32_t val;
    // Pop values from channel
    while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
    {
        // 'val' holds the received value here
    }
};

tinycoro::Task<void> Producer()
{
    // push 42 in the queue
    // if the value can not be pushed, the coroutine goes in a suspended state
    // and gets resumed, wenn the operation is succeded or the channel gets closed.
    tinycoro::EChannelOpStatus status = co_await channel.PushWait(42);

    // push 43 in the queue
    status = co_await channel.PushWait(43);
    ...

    // Close the channel when finished (this also happens in the BufferedChannel destructor)
    channel.Close();

    /* Alternatively, if this will be the last entry, you can use PushAndCloseWait.
     * This guarantees that all entries before 44 will be consumed.
     */
    // co_await channel.PushAndCloseWait(44);
}
```

### `UnbufferedChannel`

The `tinycoro::UnbufferedChannel<T>` is an asynchronous communication primitive in `tinycoro`, designed for passing messages between coroutines. It facilitates direct communication between a producer and a consumer coroutine, with operations that suspend until the counterpart is ready.

#### Constructor

```
UnbufferedChannel(std::function<void(ValueT&)> cleanupFunc = {});
```

- **`cleanupFunc`**: This is a optional cleanup function for the elements which are stucked in the channel after close was performed.

### Member Functions

#### Coroutine-Based Operations

- **PopWait:**
  ```cpp
  [[nodiscard]] auto PopWait(ValueT& val);
  ```
  Suspends the coroutine until a value is available. The value is written to `val`.

- **PushWait:**
  ```cpp
  template <typename... Args>
  [[nodiscard]] auto PushWait(Args&&... args);
  ```
  Suspends the coroutine until the value is consumed. Constructs the value in-place.

- **PushAndCloseWait:**
  ```cpp
  template <typename... Args>
  [[nodiscard]] auto PushAndCloseWait(Args&&... args);
  ```
  Pushes a value into the channel and closes it, signaling no more values will be pushed.

- **WaitForListeners:**
  ```cpp
  [[nodiscard]] auto WaitForListeners(size_t listenerCount);
  ```
  Suspends the coroutine until the specified number of listeners are present.

#### Blocking Operations

- **Push:**
  ```cpp
  template <typename... Args>
  auto Push(Args&&... args);
  ```
  Pushes a value into the channel from a non-coroutine environment. Blocks the thread until the value is consumed.

- **PushAndClose:**
  ```cpp
  template <typename... Args>
  auto PushAndClose(Args&&... args);
  ```
  Pushes a value and closes the channel from a non-coroutine environment. Blocks the thread.

#### Utility Functions

- **Close:**
  ```cpp
  void Close();
  ```
  Closes the channel, notifying all awaiters.

- **IsOpen:**
  ```cpp
  [[nodiscard]] bool IsOpen() const noexcept;
  ```
  Returns whether the channel is open.


### Example:

```cpp
#include <tinycoro/UnbufferedChannel.hpp>

// Define an unbuffered channel for int32_t values
tinycoro::UnbufferedChannel<int32_t> channel;

// Consumer coroutine that retrieves values from the channel
tinycoro::Task<void> Consumer()
{
    int32_t val;
    // Continuously pop values from the channel until it's closed
    while (tinycoro::EChannelOpStatus::CLOSED != co_await channel.PopWait(val))
    {
        // 'val' holds the received value here
    }
}

// Producer coroutine that pushes values into the channel
tinycoro::Task<void> Producer()
{
    // Push a value into the channel and wait until it's received by the consumer
    tinycoro::EChannelOpStatus status = co_await channel.PushWait(42);
    assert(status == tinycoro::EChannelOpStatus::SUCCESS);

    // Push a value and simultaneously close the channel, ensuring all prior entries are consumed
    status = co_await channel.PushAndCloseWait(44);

    if (status == tinycoro::EChannelOpStatus::LAST)
    {
        // The consumer received the last value (44), and the channel is now closed
    }
    else if (status == tinycoro::EChannelOpStatus::CLOSED)
    {
        // The value (44) was not received because the channel was already closed
    }

    // Alternatively, you can close the channel explicitly
    // channel.Close();
}
```

### `EChannelOpStatus`

The operations on `BufferedChannel` and `UnbufferedChannel` returns an `EChannelOpStatus` to indicate their outcome:

- `SUCCESS`: The operation completed successfully.
- `LAST`: Indicates the last value was received, and the channel is now closed.
- `CLOSED`: The operation failed because the channel was already closed.

## `Allocators`

Tinycoro supports **custom allocators** for controlling memory allocation of coroutine frames. This is achieved by specifying an **allocator adapter** as a template argument in the `Task` or `InlineTask` types. For example:

```cpp
tinycoro::Task<void, CustomAllocatorAdapter>
tinycoro::InlineTask<int32_t, CustomAllocatorAdapter>
```

Here’s a simple coroutine using a custom allocator adapter:

```cpp
tinycoro::Task<int32_t, AllocAdapter> Coroutine() {
    co_return 42;
}
```

---

### `AllocatorAdapter`

An **allocator adapter** is a class template that defines how memory is allocated and deallocated for the coroutine’s promise type included coroutine frame. It is passed as a **single template argument** to `Task` or `InlineTask`, and must accept exactly **one template parameter**, which is the promise type.

The adapter must define (at minimum) these two static functions:

- `operator new(size_t)` – allocates memory  
- `operator delete(void*, size_t)` – deallocates memory

If `operator new` is marked `noexcept`, the adapter must also define:

- `get_return_object_on_allocation_failure()` – to handle allocation failure gracefully.

> ⚠️ **Important:** The allocator adapter must be a class template with exactly **one** template parameter.  
> If your adapter depends on additional types (e.g., a custom allocator), you must wrap it using an alias template.  
> For example:
> ```cpp
> template<typename T>
> using Adapter = MyAdapter<T, CustomAllocatorType>;
> ```

> 💡 **Note:** This design implicitly encourages the use of **global** or **static** allocator instances.  
> This avoids dangling references or lifetime issues that can occur when coroutine frames outlive their local allocator context.  
> While this may seem restrictive, it provides a **safer** and more **predictable** memory model for asynchronous code.

A minimal example using `std::malloc` and `std::free`:

```cpp
template<typename PromiseT>
struct MallocFreeAdapter
{
    [[noreturn]] static std::coroutine_handle<PromiseT> get_return_object_on_allocation_failure()
    {
        throw std::bad_alloc{};
    }

    [[nodiscard]] static void* operator new(size_t nbytes) noexcept
    {
        return std::malloc(nbytes);
    }

    static void operator delete(void* ptr, [[maybe_unused]] size_t nbytes) noexcept
    {
        std::free(ptr);
    }
};
```

**Usage:**

```cpp
tinycoro::Task<int32_t, MallocFreeAdapter> Coroutine(int32_t val)
{
    co_return val;
}
```

---

### `Allocator`

For more advanced use cases (e.g., memory pooling or pre-allocated buffers), you can implement your own allocator class and plug it into a reusable adapter.

```cpp
// Adapter taking both Promise and custom allocator.
template <typename PromiseT, typename AllocatorT>
struct AllocatorAdapter
{
    [[nodiscard]] static void* operator new(size_t nbytes)
    {
        return AllocatorT::s_allocator.allocate_bytes(nbytes);
    }

    static void operator delete(void* ptr, size_t nbytes) noexcept
    {
        AllocatorT::s_allocator.deallocate_bytes(ptr, nbytes);
    }
};
```

Define a custom allocator and expose a usable adapter:

```cpp
template <std::unsigned_integral auto SIZE>
struct Allocator
{
    // This alias produces a single-parameter adapter.
    template<typename T>
    using adapter_t = AllocatorAdapter<T, Allocator>;

    // Adapter is a friend class,
    // so it has access to private stuffs...
    template<typename, typename>
    friend struct AllocatorAdapter;

private:
    static inline std::unique_ptr<std::byte[]>               s_buffer = std::make_unique<std::byte[]>(SIZE);
    static inline std::pmr::monotonic_buffer_resource        s_mbr{s_buffer.get(), SIZE};
    static inline std::pmr::synchronized_pool_resource       s_spr{&s_mbr};
    static inline std::pmr::polymorphic_allocator<std::byte> s_allocator{&s_spr};
};
```

**Usage:**

```cpp
// Create allocator with 10000 bytes
using AllocatorT = Allocator<10000>;

// Pass the adapter alias to the task
tinycoro::Task<void, AllocatorT::adapter_t> Coroutine()
{
    co_return;
}
```

This setup allows you to fine-tune memory usage for performance or deterministic behavior—especially useful in embedded, real-time, or resource-constrained environments.

## Warning

⚠️ Warning: Avoid `thread_local` Variables in Coroutines
In a coroutine-based environment, it is not recommended to use thread_local variables. Coroutines may be suspended and resumed on different threads, which means that the coroutine's execution could continue in a context where the thread_local variable is no longer valid or has different values. This can lead to unpredictable behavior, data races, or subtle bugs that are difficult to diagnose.

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
