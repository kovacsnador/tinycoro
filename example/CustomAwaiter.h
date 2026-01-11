#ifndef __TINY_CORO_EXAMPLE_CUSTOM_AWAITER_H__
#define __TINY_CORO_EXAMPLE_CUSTOM_AWAITER_H__

#include <tinycoro/tinycoro_all.h>

#include <string>
#include <future>

namespace third_party
{
    void async_read(std::function<void(std::string)> cb)
    {
        auto future = std::async(std::launch::async, [cb] { cb("data"); });
        future.get();
    }
}

// Your custom awaiter
struct CustomAwaiter
{
    constexpr bool await_ready() const noexcept { return false; }

    constexpr void await_suspend(auto hdl) noexcept
    {
        // We need to get the resume callback and
        // save it for later use.
        _resumeTask = tinycoro::context::PauseTask(hdl);

        auto cb = [this](std::string data) {
            // save the user data
            _userData = data;

            // resume the coroutine (you need to make them exception safe)
            _resumeTask(tinycoro::ENotifyPolicy::RESUME);
        };

        // Async third party api call
        third_party::async_read(cb);
    }

    constexpr auto await_resume() const noexcept { return _userData; }

private:
    // Custom user data (optional). Can be returned with await_resume()
    std::string _userData{};

    // Resume callback: signals the coroutine to resume.
    tinycoro::ResumeCallback_t _resumeTask;
};

tinycoro::Task<std::string> MyCoroutine()
{
    auto val = co_await CustomAwaiter{};

    // do some work after
    co_return val;
}

void Example_CustomAwaiter(tinycoro::Scheduler& scheduler)
{
    auto val = tinycoro::AllOf(MyCoroutine());
    assert(*val == "data");
}

#endif //!__TINY_CORO_EXAMPLE_CUSTOM_AWAITER_H__