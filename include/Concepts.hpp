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

        template <typename Func, typename T>
        concept Predicate = std::is_invocable_r_v<bool, Func, const T&, const T&>;

        template <typename Alloc, typename T = typename std::allocator_traits<Alloc>::value_type>
        concept AllocatorConcept = requires(typename Alloc::size_type size) {
            { size } -> std::convertible_to<std::size_t>;
        } && requires(typename Alloc::value_type value) {
            { value } -> std::convertible_to<T>;
        } && requires() {
            { Alloc() } noexcept;
        } && requires(Alloc const& b) {
            { Alloc(b) } noexcept;
        } && requires(Alloc a, std::size_t n) {
            { a.allocate(n) } -> std::convertible_to<T*>;
        } && requires(Alloc a, T* p, std::size_t n) {
            { a.deallocate(p, n) };
        };

    }  // namespace Concepts
}  // namespace ec2s

#endif
