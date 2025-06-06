// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_DIAGNOSTICS_HPP
#define TINY_CORO_DIAGNOSTICS_HPP

#ifdef TINYCORO_DIAGNOSTICS

#include <source_location>
#include <cstdio>
#include <string_view>
#include <iostream>

namespace tinycoro { namespace detail {
#if __has_include(<format>)
    inline void panic(std::string_view sv, std::source_location loc = std::source_location::current())
    {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "File: %s, func: %s, line: %d  %s\n", loc.file_name(), loc.function_name(), loc.line(), sv.cbegin());

        std::fputs(buffer, stderr);
        std::abort();
    }

#else
    inline void panic(std::string_view sv, std::source_location loc = std::source_location::current())
    {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "File: %s, func: %s, line: %d  %s\n", loc.file_name(), loc.function_name(), loc.line(), sv.cbegin());

        std::fputs(buffer, stderr);
        std::abort();
    }

#endif

}} // namespace tinycoro::detail

#define TINYCORO_ASSERT(expr) (static_cast<bool>(expr) ? void(0) : tinycoro::detail::panic("TINYCORO_ASSERT!"))
#else
#define TINYCORO_ASSERT(cond) void(0)

#endif // TINYCORO_DIAGNOSTICS

#endif // TINY_CORO_DIAGNOSTIC_HPP