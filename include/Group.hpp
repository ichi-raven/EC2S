/*****************************************************************/ /**
 * @file   Group.hpp
 * @brief  header file of Group class
 * 
 * @author ichi-raven
 * @date   February 2026
 *********************************************************************/
#ifndef EC2S_GROUP_HPP_
#define EC2S_GROUP_HPP_

#include <tuple>
#include "SparseSet.hpp"
#include "Concepts.hpp"

namespace ec2s
{
    // type erasure
    class IGroup
    {
    public:
        IGroup() = delete;

        /** 
         * @brief  constructor
         *  
         */
        IGroup(Registry& registry)
            : mRegistryRef(registry)
        {
        }

        /** 
         * @brief  destructor (virtual)
         *  
         */
        virtual ~IGroup()
        {
        }

        // TODO: check added / removed components and update the group accordingly

    private:
        Registry& mRegistryRef;
    };

    template <typename ComponentTuple>
    class Group : public IGroup
    {
    public:
        Group(Registry& registry, const ComponentTuple& componentTuple)
            : IGroup(registry)
            , mComponentTuple(componentTuple)
            , mGroupSize(0)
        {
            const auto& entities = iterateTupleAndSearchMinSizeSparseSet(mComponentTuple);
            for (const auto entity : entities)
            {
                bool included = true;
                std::apply([&included, entity](const auto&... args) { included = ((args != nullptr && args->contains(entity)) && ...); }, mComponentTuple);
                if (included)
                {
                    ++mGroupSize;
                }
            }

            /* for (std::size_t i = 0; i < mGroupSize; ++i)
            {
                std::apply([&included, entity](const auto&... args) { if () }, mComponentTuple);
            }*/
        }

        virtual ~Group() override
        {
            mRegistryRef.iterateTupleAndRemoveGroup<ComponentTuple>();
        }

        /**
         * @brief execute func on all referenced Components
         * @tparam Func type of func (to be inferred)
         * @param func function object to be executed, lambda expression, etc.
         */
        template <typename Func>
            requires Concepts::InvocableByContainerElements<Func, ComponentTuple>
        void each(Func func)
        {
            auto vectorTuple = ToComponentVectorTuple(std::make_index_sequence<std::tuple_size_v<ComponentTuple>>{});

            for (std::size_t i = 0; i < mGroupSize; ++i)
            {
                std::apply([&func, i](auto&... args) { func(args[i]...); }, vectorTuple);
            }
        }

        /**
         * @brief execute system on all referenced Components that takes Entity as its first argument
         * @tparam Func type of func (to be inferred)
         * @param func function object to be executed, lambda expression, etc.
         */
        template <typename Func>
            requires Concepts::InvocableWithEntityByContainerElements<Func, ComponentTuple>
        void each(Func func)
        {
            auto vectorTuple = ToComponentVectorTuple(std::make_index_sequence<std::tuple_size_v<ComponentTuple>>{});

            auto entities = std::get<0>(mComponentTuple)->getDenseEntities();

            for (std::size_t i = 0; i < mGroupSize; ++i)
            {
                std::apply([&func, i](auto&... args) { func(entities[i], args[i]...); }, vectorTuple);
            }
        }

    private:
        template <size_t N = 0, typename TupleType>
        const std::vector<Entity>& iterateTupleAndSearchMinSizeSparseSet(TupleType& t, ISparseSet* pMinSparseSet = nullptr)
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

        template <std::size_t... Is>
        auto ToComponentVectorTuple(std::index_sequence<Is...>)
        {
            return std::tuple<std::vector<std::tuple_element_t<Is, ComponentTuple>>&...>{ std::get<Is>(mComponentTuple)->getPackedVector()... };
        }

        ComponentTuple mComponentTuple;
        std::size_t mGroupSize;
    };
}  // namespace ec2s

#endif
