/*****************************************************************/ /**
 * @file   Condition.hpp
 * @brief  header file of Condition type information class for View
 * 
 * @author ichi-raven
 * @date   January 2026
 *********************************************************************/
#ifndef EC2S_CONDITION_HPP_
#define EC2S_CONDITION_HPP_

#include <type_traits>
#include <concepts>
#include <tuple>    

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

    // concept
    template <typename T>
    concept ExcludeType = ComponentCondition<std::remove_cvref_t<T>>::value;

    template <typename T>
    using TupleType = ComponentCondition<std::remove_cvref_t<T>>::TupleType;

    template <typename T>
    using ExcludeTupleType = ComponentCondition<std::remove_cvref_t<T>>::ExcludeTupleType;


}  // namespace ec2s

#endif