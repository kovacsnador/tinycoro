// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_RESUME_SIGNAL_EVENT_HPP
#define TINY_CORO_RESUME_SIGNAL_EVENT_HPP

#include <atomic>

#include "Common.hpp"
#include "CallOnce.hpp"

namespace tinycoro {

    namespace detail {

        // This class is intented to handle awaitable
        // resumption from a paused state.
        struct ResumeSignalEvent
        {
        private:
            // Need to be up there to make the
            // "requirement" work for Set().
            ResumeCallback_t    _notifyCallback;
            mutable std::atomic_flag _flag;

        public:
            ResumeSignalEvent() = default;

            ResumeSignalEvent(ResumeSignalEvent&& other) noexcept
            : _notifyCallback{std::move(other._notifyCallback)}
            {
            }

            auto Notify(ENotifyPolicy policy = ENotifyPolicy::RESUME) const
            {
                assert(_notifyCallback);

                // We notify the coroutine.
                //
                // This notify callback is responsible
                // for the corouitne resumption.
                return CallOnce(_flag, std::memory_order::acquire, _notifyCallback, policy);
            }

            // Sets the notify callback
            template <typename T>
                requires requires (T&& t) {
                    { _notifyCallback = std::forward<T>(t) };
                }
            void Set(T&& cb)
            {
                assert(_flag.test() == false);

                _notifyCallback = std::forward<T>(cb);
            }
        };

    } // namespace detail

} // namespace tinycoro

#endif // TINY_CORO_RESUME_SIGNAL_EVENT_HPP