// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

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