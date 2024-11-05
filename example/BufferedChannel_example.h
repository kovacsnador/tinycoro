#ifndef __TINY_CORO_EXAMPLE_BUFFERED_CHANNEL_H__
#define __TINY_CORO_EXAMPLE_BUFFERED_CHANNEL_H__

#include <tinycoro/tinycoro_all.h>

#include "Common.h"

void processData(const auto& data)
{
    for (const auto& value : data)
    {
        SyncOut() << "  Thread: " << std::this_thread::get_id() << "  Value: " << value << '\n';
    }
}

void Example_bufferedChannel(auto& scheduler)
{
    SyncOut() << "\n\nExample_bufferedChannel:\n";

    tinycoro::BufferedChannel<std::vector<int32_t>> channel;

    auto consumer = [&]() -> tinycoro::Task<void> {
        std::vector<int32_t> data;
        while (tinycoro::BufferedChannel_OpStatus::CLOSED != co_await channel.PopWait(data))
        {
            processData(data);
        }
    };

    channel.Push({1, 2, 3});
    channel.Push({1, 2, 3});
    channel.Push({4, 5, 6});          
    channel.Push({7});
    channel.PushAndClose({8, 9});   // push and close the channel

    tinycoro::GetAll(scheduler, consumer());
}

#endif //!__TINY_CORO_EXAMPLE_BUFFERED_CHANNEL_H__