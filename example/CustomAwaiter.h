#ifndef __TINY_CORO_EXAMPLE_CUSTOM_AWAITER_H__
#define __TINY_CORO_EXAMPLE_CUSTOM_AWAITER_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

// Your custom awaiter
struct CustomAwaiter
{
    constexpr bool await_ready() const noexcept { return false; }

    void await_suspend(auto hdl) noexcept
    {
        // save resume task callback
        _resumeTask = tinycoro::context::PauseTask(hdl);

        auto cb = [](void* userData, [[maybe_unused]] int i) {

            SyncOut() << "  Callback called... " << i << " Thread id: " << std::this_thread::get_id() << '\n';

            auto self = static_cast<decltype(this)>(userData);

            // do some work
            std::this_thread::sleep_for(100ms);
            self->_userData++;

            // resume the coroutine (you need to make them exception safe)
            self->_resumeTask(tinycoro::ENotifyPolicy::RESUME);
        };

        AsyncCallbackAPIvoid(cb, this);
    }

    constexpr auto await_resume() const noexcept { return _userData; }

    int32_t _userData{41};

    tinycoro::ResumeCallback_t _resumeTask;
};

void Example_CustomAwaiter(auto& scheduler)
{
    SyncOut() << "\n\nExample_CustomAwaiter:\n";

    auto asyncTask = []() -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';
        // do some work before

        auto val = co_await CustomAwaiter{};

        // do some work after
        co_return val;
    };

    auto val = tinycoro::AllOf(scheduler, asyncTask());

    SyncOut() << "co_return => " << *val << '\n'; 
}

#endif //!__TINY_CORO_EXAMPLE_CUSTOM_AWAITER_H__