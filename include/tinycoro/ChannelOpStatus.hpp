#ifndef __TINY_CORO_CHANNEL_OP_STATUS_HPP__
#define __TINY_CORO_CHANNEL_OP_STATUS_HPP__

namespace tinycoro {

    enum class EChannelOpStatus
    {
        SUCCESS,
        LAST,
        CLOSED
    };

} // namespace tinycoro

#endif //!__TINY_CORO_CHANNEL_OP_STATUS_HPP__