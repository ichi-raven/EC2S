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
    class Registry;

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

    protected:
        // TODO: check added / removed components and update the group accordingly

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
            mRegistryRef.template iterateTupleAndRegisterGroup<ComponentTuple>(this);

            const auto& entities = iterateTupleAndSearchMinSizeSparseSet(mComponentTuple);

            for (const auto entity : entities)
            {
                bool includeAll = false;
                std::apply([&includeAll, entity](const auto&... args) { includeAll = (args->contains(entity) && ...); }, mComponentTuple);
                if (includeAll)
                {
                    // swap with the "mGroupSize"-th element and increment mGroupSize
                    iterateTupleAndSwap(mComponentTuple, entity, mGroupSize);
                    ++mGroupSize;
                }
            }
        }

        virtual ~Group() override
        {
            mRegistryRef.template iterateTupleAndRemoveGroup<ComponentTuple>();
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
            for (std::size_t i = 0; i < mGroupSize; ++i)
            {
                std::apply([&func, i](auto&... args) { func(args->getPackedVector()[i]...); }, mComponentTuple);
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
            auto entities = std::get<0>(mComponentTuple)->getDenseEntities();

            for (std::size_t i = 0; i < mGroupSize; ++i)
            {
                std::apply([&func, i](auto&... args) { func(entities[i], args->getPackedVector()[i]...); }, mComponentTuple);
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

        template <size_t N = 0, typename TupleType>
        void iterateTupleAndSwap(TupleType& t, const Entity target, const std::size_t groupSize)
        {
            if constexpr (N < std::tuple_size<TupleType>::value)
            {
                auto* pSparseSet = std::get<N>(t);
                pSparseSet->swap(target, pSparseSet->getDenseEntities()[groupSize]);
                iterateTupleAndSwap<N + 1>(t, target, groupSize);
            }
        }

        ComponentTuple mComponentTuple;
        std::size_t mGroupSize;
    };
}  // namespace ec2s

#endif
