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

void AsyncCallbackAPIvoid(std::function<void(void*, int)> cb, void* userData)
{
    std::jthread t{[cb, userData] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(200ms);
        cb(userData, 42);
    }};
    t.detach();
}

std::jthread AsyncCallbackAPIFunctionObject(std::function<void(int, bool)> cb, [[maybe_unused]] int flag)
{
    return std::jthread{[cb, flag] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(200ms);
        cb(42, flag);
    }};
}

void AsyncCallbackAPIFunctionObjectVoid(std::function<void(int, bool)> cb, [[maybe_unused]] int flag)
{
    std::jthread t{[cb, flag] {
        SyncOut() << "  AsyncCallbackAPI... Thread id: " << std::this_thread::get_id() << '\n';
        std::this_thread::sleep_for(200ms);
        cb(42, flag);
    }};
    t.detach();
}

#endif //!__TINY_CORO_EXAMPLE_COMMON_H__