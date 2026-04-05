# Modernizing libcurl with coroutines

This **very basic** example demonstrates how to modernize **libcurl** with **C++ coroutines** using Tinycoro.

## What this example demonstrates

* Driving **multiple HTTP transfers concurrently** with libcurl multi
* Suspending coroutines while a transfer is in progress
* Resuming the waiting coroutine when libcurl reports completion
* Using `tinycoro::BufferedChannel` as a lightweight work queue
* Running the libcurl event loop itself as a Tinycoro task
* Coordinating several requests with `tinycoro::AllOfAwait`

## Architecture

The flow is intentionally simple:

```text
request coroutine
    ↓
create CURL easy handle
    ↓
queue handle into CurlMulti
    ↓
CurlMulti::Run() drives libcurl
    ↓
libcurl reports completed transfer
    ↓
Tinycoro event is signaled
    ↓
waiting coroutine resumes
```

The key idea is that each request coroutine creates a small synchronization event and stores its pointer inside the CURL easy handle using `CURLOPT_PRIVATE`.

When `CurlMulti::Run()` receives a completed transfer from `curl_multi_info_read`, it extracts that pointer and signals the event, which resumes the suspended coroutine.

This pattern is a good blueprint for adapting **other callback- or event-based libraries** to Tinycoro.

## Main components

### `Easy`

A lightweight wrapper around a single CURL easy handle.

Responsibilities:

* configure the URL and callbacks
* collect response data into a `std::string`
* attach coroutine wake-up state through `CURLOPT_PRIVATE`
* expose an awaitable request interface

Each `Easy` instance represents **one HTTP request**.

### `CurlMulti`

A Tinycoro-friendly wrapper around the CURL multi interface.

Responsibilities:

* own the multi handle
* receive queued easy handles
* drive `curl_multi_poll` / `curl_multi_perform`
* detect completed transfers
* wake the suspended request coroutine

`CurlMulti::Run()` is itself a coroutine task, which makes the event loop naturally fit into Tinycoro’s scheduler.

## Why this example matters

This sample is about more than HTTP requests. It shows one practical way to **modernize libcurl with coroutines** without hiding or replacing libcurl itself.

libcurl already provides a powerful event-driven API through its multi interface. What coroutines add is a cleaner way to express the control flow around it.

It also demonstrates a broader **integration pattern**: Tinycoro can be used together with mature C libraries that already expose asynchronous or event-driven APIs.

The same coroutine adaptation technique can be reused for:

* sockets
* database clients
* message queues
* file watchers
* GUI event systems
* custom C callback APIs

## Build requirements

This example can only built when **libcurl** is available.

CMake automatically checks for:

* `CURL::libcurl`

## Suggested reading order

If you are new to Tinycoro, start with the simpler examples first.

This example is best viewed as an **advanced integration tutorial** that explains how to connect Tinycoro to external event loops and callback-based systems.
