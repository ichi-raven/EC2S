/*****************************************************************/ /**
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
#include "Concepts.hpp"

namespace ec2s
{
    /**
     * @brief  View class, generated from Registry, providing each for multiple Components
     * @tparam ComponentType type of ComponentData this View refers to (at least one)
     * @tparam OtherComponentTypes for multiple Component types
     */
    template <typename IncludeTuple, typename ExcludeTuple>
    class View
    {
    public:
        /** 
         * @brief constructor
         * @details user does not need to build this
         * @param head reference to SparseSet of the first ComponentType
         * @param tails reference to the SparseSet of the subsequent ComponentType
         */
        View(const IncludeTuple& includeTuple, const ExcludeTuple& excludeTuple)
            : mSparseSets(includeTuple)
            , mExcludeSparseSets(excludeTuple)
        {
            static_assert(std::tuple_size_v<IncludeTuple> != 0, "View must include at least one Component type!");
        }

        /**
         * @brief execute func on all referenced Components
         * @tparam Func type of func (to be inferred)
         * @param func function object to be executed, lambda expression, etc.
         */
        template <typename Func>
            requires Concepts::InvocableByContainerElements<Func, IncludeTuple>
        void each(Func func)
        {
            const auto& entities = iterateTupleAndSearchMinSizeSparseSet(mSparseSets);

            for (const auto entity : entities)
            {
                if constexpr (std::tuple_size_v<ExcludeTuple> != 0)
                {
                    // check Exclude components
                    bool excluded = false;
                    std::apply([&excluded, entity](const auto&... args) { excluded = ((args && args->contains(entity)) || ...); }, mExcludeSparseSets);

                    if (excluded)
                    {
                        continue;
                    }
                }

                std::apply([&func, entity](auto&... args) { func(args->get(entity)...); }, mSparseSets);
            }
        }

        /**
         * @brief execute system on all referenced Components that takes Entity as its first argument
         * @tparam Func type of func (to be inferred)
         * @param func function object to be executed, lambda expression, etc.
         */
        template <typename Func>
            requires Concepts::InvocableWithEntityByContainerElements<Func, IncludeTuple>
        void each(Func func)
        {
            const auto& entities = iterateTupleAndSearchMinSizeSparseSet(mSparseSets);

            for (const auto entity : entities)
            {
                if constexpr (std::tuple_size_v<ExcludeTuple> != 0)
                {
                    // check Exclude components
                    bool excluded = false;
                    std::apply([&excluded, entity](const auto&... args) { excluded = ((args && args->contains(entity)) || ...); }, mExcludeSparseSets);

                    if (excluded)
                    {
                        continue;
                    }
                }

                std::apply([&func, entity](auto&... args) { func(entity, args->get(entity)...); }, mSparseSets);
            }
        }

    private:
        template <size_t N = 0, typename TupleType>
        const std::pmr::vector<Entity>& iterateTupleAndSearchMinSizeSparseSet(TupleType& t, ISparseSet* pMinSparseSet = nullptr)
        {
            if constexpr (N < std::tuple_size<TupleType>::value)
            {
                // TODO: static_assert to check tuple_element_t

                if (pMinSparseSet == nullptr || pMinSparseSet->size() > std::get<N>(t)->size())
                {
                    pMinSparseSet = std::get<N>(t);
                }

                return iterateTupleAndSearchMinSizeSparseSet<N + 1>(t, pMinSparseSet);
            }

            assert(pMinSparseSet != nullptr || !"TupleType must have at least one element!");
            return pMinSparseSet->getDenseEntities();
        }

        //! tuple of all View configuration components
        IncludeTuple mSparseSets;
        ExcludeTuple mExcludeSparseSets;
    };
}  // namespace ec2s

#endif