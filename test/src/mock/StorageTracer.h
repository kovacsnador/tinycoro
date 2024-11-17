#ifndef __TINY_CORO_TEST_SRC_MOCK_STORAGE_TRACER_H__
#define __TINY_CORO_TEST_SRC_MOCK_STORAGE_TRACER_H__

#include <string>

#include <gmock/gmock.h>

namespace tinycoro { namespace test {
    
    struct StorageTracer
    {
        StorageTracer() { defaultConstructor++; }

        StorageTracer(std::string s)
        : str{std::move(s)}
        {
            constructor++;
        }

        StorageTracer(const StorageTracer& other)
        : str{other.str}
        {
            copyConstructor++;
        }

        StorageTracer(StorageTracer&& other) noexcept
        : str{std::move(other.str)}
        {
            moveConstructor++;
        }

        StorageTracer& operator=(const StorageTracer& other)
        {
            str = other.str;
            copyAssign++;
            return *this;
        }

        StorageTracer& operator=(StorageTracer&& other) noexcept
        {
            str = std::move(other.str);
            moveAssign++;
            return *this;
        }

        ~StorageTracer() { Destructor(); }

        size_t defaultConstructor{};
        size_t constructor{};
        size_t copyConstructor{};
        size_t moveConstructor{};
        size_t copyAssign{};
        size_t moveAssign{};

        MOCK_METHOD(void, Destructor, ());

        std::string str{};
    };
}} // namespace tinycoro::test

#endif //!__TINY_CORO_TEST_SRC_MOCK_STORAGE_TRACER_H__