#ifndef TINY_CORO_CHANNEL_OP_STATUS_HPP
#define TINY_CORO_CHANNEL_OP_STATUS_HPP

namespace tinycoro {

    enum class EChannelOpStatus
    {
        SUCCESS,
        LAST,
        CLOSED
    };

} // namespace tinycoro

#endif // TINY_CORO_CHANNEL_OP_STATUS_HPP