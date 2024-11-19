#ifndef __TINY_CORO_DYNAMIC_STORAGE_HPP__
#define __TINY_CORO_DYNAMIC_STORAGE_HPP__

#include <memory>
#include <cassert>

#include "Exception.hpp"

namespace tinycoro { namespace detail {

    template <typename InterpreterClassT>
    struct DynamicStorage
    {
        DynamicStorage() = default;

        template <typename ClassT, typename... Args>
            requires std::constructible_from<ClassT, Args...>
            && (std::derived_from<ClassT, InterpreterClassT> || std::same_as<ClassT, InterpreterClassT>)
        ClassT* Construct(Args&&... args)
        {
            if (_smartPtr)
            {
                throw DynamicStorageException{"Storage object is already constructed. Use Assign(...)"};
            }

            _smartPtr = std::make_unique<ClassT>(std::forward<Args>(args)...);

            return static_cast<ClassT*>(_smartPtr.get());
        }

        template <typename ClassT, typename Arg>
            requires std::assignable_from<ClassT&, Arg&&> && (std::derived_from<ClassT, InterpreterClassT> || std::same_as<ClassT, InterpreterClassT>)
        ClassT* Assign(Arg&& arg)
        {
            if (_smartPtr == nullptr)
            {
                throw DynamicStorageException{"Storage object is NOT constructed yet. Use Construct(...) first."};
            }

            *_smartPtr = std::forward<Arg>(arg);

            // debug check for type safety
            assert(dynamic_cast<ClassT*>(_smartPtr.get()) != nullptr);

            return static_cast<ClassT*>(_smartPtr.get());
        }

        void reset() { _smartPtr.reset(); }

        auto* operator->() { return _smartPtr.get(); }

        const auto* operator->() const { return _smartPtr.get(); }

        operator bool() const noexcept { return _smartPtr != nullptr; }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept { return !this->operator bool(); }

    private:
        std::unique_ptr<InterpreterClassT> _smartPtr{};
    };
}} // namespace tinycoro::detail

#endif //!__TINY_CORO_DYNAMIC_STORAGE_HPP__