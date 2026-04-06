#include <curl/curl.h>
#include <tinycoro/tinycoro_all.h>

#include <iostream>

#include "include/CurlEasy.hpp"
#include "include/CurlMulti.hpp"

// This is out start/root corouitne.
auto main_coroutine() -> tinycoro::Task<>
{
    CurlMulti multi;

    tinycoro::Scheduler scheduler;
    tinycoro::TaskGroup<> taskGroup;

    // We start the event loop Run() on the provided scheduler.
    taskGroup.Spawn(scheduler, multi.Run());

    // Start three HTTP requests concurrently and wait until all of them finish.
    auto [result1, result2, result3] = co_await tinycoro::AllOfAwait(scheduler,
                                                                     FetchUrl("https://example.com/", multi),
                                                                     FetchUrl("https://example.org/", multi), 
                                                                     FetchUrl("https://example.net/", multi));

    std::cout << *result1 << '\n'
              << *result2 << '\n'
              << *result3 << '\n';

    // Stop the background event loop once no more requests are needed.
    taskGroup.CancelAll();
    multi.Notify();

    // this Join() is not strictly necessary, just to make sure we are not blocking here.
    co_await taskGroup.Join();
}

int main()
{
    // libcurl requires one process-wide initialization before any CURL handle is used.
    curl_global_init(CURL_GLOBAL_DEFAULT);

    tinycoro::AllOf(main_coroutine());
    
    // Matching global cleanup after all requests are complete.
    curl_global_cleanup();

    return 0;
}
