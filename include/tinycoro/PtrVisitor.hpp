// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------

#ifndef TINY_CORO_PTR_VISITOR_HPP
#define TINY_CORO_PTR_VISITOR_HPP

#include <variant>
#include <type_traits>

namespace tinycoro { namespace detail {

    namespace local {

        // Removes cv-qualifiers, references, and a single level of pointer.
        // Used to normalize variant alternatives so that T and T* compare equal.
        template <typename T>
        struct CleanTypeT
        {
            using type = std::remove_cvref_t<std::remove_pointer_t<T>>;
        };

        template <typename T>
        using CleanType_t = CleanTypeT<T>::type;

        // Primary template: rejects all non-std::variant types by default.
        template <typename... Ts>
        struct ValidatePtrVariantT : std::false_type
        {
        };

        // Accepts std::variant where all alternatives normalize to the same base type
        // after removing cv/ref and pointer qualifiers (e.g. T, T*, const T*).
        //
        // Before using it, make sure that you remove all the top qualifiers
        // from the variant type. 
        // Use e.g. std::remove_cvref_t
        template <typename FirstT, typename... Ts>
        struct ValidatePtrVariantT<std::variant<FirstT, Ts...>>
        : std::bool_constant<(std::is_same_v<local::CleanType_t<FirstT>, local::CleanType_t<Ts>> && ...)>
        {
        };

        template<typename... ArgsT>
        static constexpr auto ValidatePtrVariant_v = ValidatePtrVariantT<ArgsT...>::value;

    } // namespace local

    // Visits a validated std::variant<T, T*> and always returns a pointer to T.
    // The returned pointer is const-qualified if the variant itself is const.
    template <typename VariantT>
        requires local::ValidatePtrVariant_v<std::remove_cvref_t<VariantT>>
    auto PtrVisit(VariantT& variant)
    {
        assert(variant.index() != std::variant_npos);
        assert(variant.valueless_by_exception() == false);

        // Base value type of the variant (T), independent of pointer or cv-qualification.
        using value_t = std::remove_pointer_t<std::variant_alternative_t<0, VariantT>>;

        // Return type: pointer to T, propagating constness from the variant.
        using result_t = std::conditional_t<std::is_const_v<VariantT>, const value_t*, value_t*>;

        return std::visit(
            // Visitor always returns result_t for all alternatives.
            []<typename T>(T& v) -> result_t {
                if constexpr (std::is_pointer_v<T>)
                {
                    // Pointer alternative:
                    // Rebind constness to the pointee by dereferencing first.
                    return std::addressof(*v);
                }   
                else
                {
                    // Value alternative:
                    // Take the address directly.
                    return std::addressof(v);
                }
            },
            variant);
    }

}} // namespace tinycoro::detail

#endif // TINY_CORO_PTR_VISITOR_HPP