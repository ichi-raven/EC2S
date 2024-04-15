/*****************************************************************//**
 * @file   View.hpp
 * @brief  header file of View class
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_VIEW_HPP_
#define EC2S_VIEW_HPP_

#include <tuple>

#include "SparseSet.hpp"

namespace ec2s
{
    /**
     * @brief  View class, generated from Registry, providing each for multiple Components
     * @tparam ComponentType Type of ComponentData this View refers to (at least one)
     * @tparam OtherComponentTypes For multiple Component types
     */
    template<typename ComponentType, typename... OtherComponentTypes>
    class View
    {
    public:

        /** 
         * @brief Constructor
         * @details User does not need to build this
         * @param head Reference to SparseSet of the first ComponentType
         * @param tails Reference to the SparseSet of the subsequent ComponentType
         */
        View(SparseSet<ComponentType>& head, SparseSet<OtherComponentTypes>&... tails)
            : mSparseSets(head, tails...)
        {}

        /**
        * @brief Returns the number of elements in the referenced SparseSet with the lowest number of elements
         * @details i.e., each() is executed this many times
         */
        std::size_t getMinSize() const
        {
            return searchMinSize<OtherComponentTypes...>(std::get<0>(mSparseSets).size());
        }

        /**
       * @brief Execute func on all referenced Components
         * @tparam Func type of func (to be inferred)
         * @tparam Traits::IsEligibleEachFunc user doesn't need to pass this, the Func type takes the referenced Component as argument and executable or not
         * @param func Function object to be executed, lambda expression, etc.
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, false, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /**
         * @brief  Execute func on all referenced Components with entity ID
         * @tparam Func type of func (to be inferred)
         * @tparam Traits::IsEligibleEachFunc user doesn't need to pass this, the Func type takes the referenced Component as argument and executable or not
         * @param func Function object to be executed, lambda expression, etc.
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, true, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /** 
         * @brief Execute func with its Entity for all referenced Components
         * @tparam Func type of func (to be inferred)
         * @tparam Traits::IsEligibleEachFunc user does not need to pass this, whether the func type can take Entity and referenced Component as arguments and execute it
         * @param func Function object to be executed, lambda expression, etc.
         */
        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, false, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /**
         * @brief Execute func on the specified Component referenced by View
         * @tparam TargetComponentType The first (one or more) of the specified Component types
         * @tparam OtherTargetComponentTypes Subsequent Component types
         * @tparam Func Func type (to be inferred)
         * @tparam Traits::IsEligibleEachFunc user does not need to pass this, whether the func type can take Entity and the referenced Component as arguments and execute it
         * @param func Function object to be executed, lambda expression, etc.
         */
        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, true, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

    private:

        /**
         * @brief  Finds the smallest SparseSet in the mSparseSets tuple and returns its size
         */
        template<typename Head, typename... Tail>
        std::size_t searchMinSize(const std::size_t nowMin) const 
        {
            return std::min(std::get<SparseSet<Head>&>(mSparseSets).size(), std::get<SparseSet<Tail>&>(mSparseSets).size()...);
        }

        /**
         * @brief  Finds the minimum size SparseSet from the hash value and invokeIfValidEntity on its DenseEntities
         */
        template<typename Head, typename... Tail>
        TypeHash searchMinSizeSparseSet(const std::size_t nowMin, const TypeHash hash) const
        {
            auto size = std::get<SparseSet<Head>&>(mSparseSets).size();

            if constexpr (sizeof...(Tail) > 0)
            {
                if (nowMin <= size)
                {
                    return searchMinSizeSparseSet<Tail...>(nowMin, hash);
                }
                else
                {
                    return searchMinSizeSparseSet<Tail...>(size, TypeHasher::hash<Head>());
                }
            }
            else
            {
                return (nowMin <= size ? hash : TypeHasher::hash<Head>());
            }
        }

        /**
         * @brief  Find the minimum size SparseSet in the hash value and invokeIfValidEntity on its DenseEntities
         */
        template<typename Func, size_t SSIndex, bool withEntity, typename... ComponentTypes>
        void invokeByMinSizeSparseSet(Func func, TypeHash hash)
        {
            std::tuple targetTuple = std::tuple<SparseSet<ComponentTypes>&...>(std::get<SparseSet<ComponentTypes>&>(mSparseSets)...);

            if constexpr (SSIndex < sizeof...(OtherComponentTypes) + 1)
            {
                if (auto& sparseSet = std::get<SSIndex>(mSparseSets); hash == sparseSet.getPackedTypeHash())
                {
                    invokeIfValidEntity<withEntity>(func, sparseSet.getDenseEntities(), targetTuple, std::index_sequence_for<ComponentTypes...>()); 
                }
                else
                {
                    invokeByMinSizeSparseSet<Func, SSIndex + 1, withEntity, ComponentTypes...>(func, hash);
                }
            }
        }

        /**
         * @brief  For each specified SparseSet, executes func with the entities that were valid in all of the entities
         */
        template<bool withEntity, typename T, T... I, typename Func, typename Tuple>
        void invokeIfValidEntity(Func func, const std::vector<Entity>& entities, Tuple& tuple, std::integer_sequence<T, I...>)
        {
            static std::size_t sparseIndices[sizeof...(I)] = { 0 };

            for (const auto& entity : entities)
            {
                if (!(std::get<I>(tuple).getSparseIndexIfValid(entity, sparseIndices[I]) & ...))
                {
                    continue;
                }

                if constexpr (withEntity)
                {
                    func(entity, std::get<I>(tuple).getBySparseIndex(sparseIndices[I], entity)...);
                }
                else
                {
                    func(std::get<I>(tuple).getBySparseIndex(sparseIndices[I], entity)...);
                }
            }
        }

        std::tuple<SparseSet<ComponentType>&, SparseSet<OtherComponentTypes>&...> mSparseSets;
    };
}

#endif