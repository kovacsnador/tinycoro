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

    namespace impl {
        [[noreturn]] static void panicImpl(const auto& msg) noexcept
        {
            std::fputs(msg, stderr);
            std::abort();
        }
    } // namespace impl

#if __has_include(<format>)

#include <format>

    template <typename... Args>
    struct FormatStringLog
    {
        template <typename StringT>
            requires std::constructible_from<std::format_string<Args...>, StringT>
        consteval FormatStringLog(StringT string, std::source_location loc = std::source_location::current())
        : formatString{string}
        , location{loc}
        {
        }

        std::format_string<Args...> formatString;
        std::source_location        location;
    };

    // Use std::type_identity_t to prevent type deduction
    // based on the FormatStringLog class.
    template <typename... Args>
    [[noreturn]] static inline void panic(FormatStringLog<std::type_identity_t<Args>...> fmt, Args&&... args) noexcept
    {
        auto buffer = std::format("Func: {}, line: {} \"{}\"\n",
                                  fmt.location.function_name(),
                                  fmt.location.line(),
                                  std::format(fmt.formatString, std::forward<Args>(args)...));

        impl::panicImpl(buffer.c_str());
    }

#else
    struct SimpleLog
    {
        template<typename StringT>
            requires std::constructible_from<std::string_view, StringT>
        SimpleLog(const StringT& s, std::source_location loc = std::source_location::current())
        : fmt{s}
        , location{loc}
        { 
        }

        std::string_view fmt;
        std::source_location location;
    };
    
    // This exist only to maintain compatibility
    // with the format library, but it completly ignores
    // the passed extra arguments.
    template<typename... Args>
    static inline void panic(SimpleLog log, Args...)
    {
        char buffer[512] = {};
        snprintf(buffer, sizeof(buffer), "Func: %s, line: %d  %s\n", log.location.function_name(), log.location.line(), log.fmt.data());

        impl::panicImpl(buffer);
    }

#endif /* __has_include(<format>) */

}} // namespace tinycoro::detail

#define TINYCORO_ASSERT(expr) (static_cast<bool>(expr) ? void(0) : tinycoro::detail::panic("TINYCORO_ASSERT!"))
#define TINYCORO_PANIC(...) tinycoro::detail::panic(__VA_ARGS__)
#else
#define TINYCORO_ASSERT(cond) void(0)
#define TINYCORO_PANIC(...)

#endif // TINYCORO_DIAGNOSTICS

#endif // TINY_CORO_DIAGNOSTIC_HPP