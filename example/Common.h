#ifndef __TINY_CORO_EXAMPLE_COMMON_H__
#define __TINY_CORO_EXAMPLE_COMMON_H__

#include <syncstream>
#include <iostream>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

auto SyncOut(std::ostream& stream = std::cout)
{
    return std::osyncstream{stream};
}

typedef void (*funcPtr)(void*, int, int);

std::jthread AsyncCallbackAPI(void* userData, funcPtr cb, int i = 43)
{
    return std::jthread{[cb, userData, i] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(200ms);
        cb(userData, 42, i);
    }};
}

void AsyncCallbackAPIvoid(std::regular_invocable<void*, int> auto cb, void* userData)
{
    std::jthread t{[cb, userData] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(200ms);
        cb(userData, 42);
    }};
    t.detach();
}

#endif //!__TINY_CORO_EXAMPLE_COMMON_H__