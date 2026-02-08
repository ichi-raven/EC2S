/*****************************************************************/ /**
 * @file   Concepts.hpp
 * @brief  header file of concept definitions
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_CONCEPTS_HPP_
#define EC2S_CONCEPTS_HPP_

#include "Entity.hpp"

#include <concepts>
#include <tuple>
#include <type_traits>

namespace ec2s
{
    //! namespace for all concepts
    namespace Concepts
    {
        template <typename Func, typename... Types>
        concept Invocable = std::is_invocable_v<Func, Types&...>;

        template <typename Func, typename... Types>
        concept InvocableWithEntity = std::is_invocable_v<Func, Entity, Types&...>;

        template <typename Func, typename Tuple>
        concept InvocableByContainerElements = []<typename... Cs>(std::tuple<Cs...>*) { return std::invocable<Func, typename std::remove_pointer_t<Cs>::ComponentType&...>; }(static_cast<Tuple*>(nullptr));

        template <typename Func, typename Tuple>
        concept InvocableWithEntityByContainerElements = []<typename... Cs>(std::tuple<Cs...>*) { return std::invocable<Func, Entity, typename std::remove_pointer_t<Cs>::ComponentType&...>; }(static_cast<Tuple*>(nullptr));
    
    }  // namespace Concepts
}  // namespace ec2s

#endif
