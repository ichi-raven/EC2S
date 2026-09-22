/*****************************************************************/ /**
 * @file   Condition.hpp
 * @brief  header file of Condition type information class for View
 * 
 * @author ichi-raven
 * @date   January 2026
 *********************************************************************/
#ifndef EC2S_CONDITION_HPP_
#define EC2S_CONDITION_HPP_

#include <concepts>
#include <memory>
#include <tuple>
#include <type_traits>

#include "Concepts.hpp"

namespace ec2s
{
    template <typename T>
    struct Exclude
    {
        using type = T;
    };

    template <typename T>
    class SparseSet;

    template <typename T>
    struct ComponentCondition : std::false_type
    {
        using TupleType        = std::tuple<SparseSet<T>*>;
        using ExcludeTupleType = std::tuple<>;
    };

    template <typename U>
    struct ComponentCondition<Exclude<U>> : std::true_type
    {
        using TupleType        = std::tuple<>;
        using ExcludeTupleType = std::tuple<SparseSet<U>*>;
    };

    template <typename T>
    struct ConstComponentCondition : std::false_type
    {
        using TupleType        = std::tuple<const SparseSet<T>*>;
        using ExcludeTupleType = std::tuple<>;
    };

    template <typename U>
    struct ConstComponentCondition<Exclude<U>> : std::true_type
    {
        using TupleType        = std::tuple<>;
        using ExcludeTupleType = std::tuple<const SparseSet<U>*>;
    };

    // concept
    template <typename T>
    concept ExcludeType = ComponentCondition<std::remove_cvref_t<T>>::value;

    template <typename T>
    using TupleType = ComponentCondition<std::remove_cvref_t<T>>::TupleType;

    template <typename T>
    using ExcludeTupleType = ComponentCondition<std::remove_cvref_t<T>>::ExcludeTupleType;

    template <typename T>
    using ConstTupleType = ConstComponentCondition<std::remove_cvref_t<T>>::TupleType;

    template <typename T>
    using ConstExcludeTupleType = ConstComponentCondition<std::remove_cvref_t<T>>::ExcludeTupleType;

}  // namespace ec2s

#endif