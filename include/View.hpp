/*****************************************************************//**
 * @file   View.hpp
 * @brief  Viewクラスのヘッダファイル
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
    template<typename ComponentType, typename... OtherComponentTypes>
    class View
    {
    public:
        View(SparseSet<ComponentType>& head, SparseSet<OtherComponentTypes>&... tails)
            : mSparseSets(head, tails...)
        {

        }

        std::size_t getMinSize() const
        {
            return searchMinSize<OtherComponentTypes...>(std::get<0>(mSparseSets).size());
        }

        template<typename Func, typename Traits::IsEligibleEachFunc<Func, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            // 最もサイズの小さいSparseSetのDenseIndicesをイテレート
            execEachByMinSizeSparseSet<Func, 0, false, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
        }

        template<typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            // 最もサイズの小さいSparseSetのDenseIndicesをイテレート
            execEachByMinSizeSparseSet<Func, 0, true, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
        }



        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            //if constexpr (sizeof...(OtherTargetComponentTypes) == 0)
            //{
                execEachByMinSizeSparseSet<Func, 0, false, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
            //}
            //else
            //{
            //    //execEachByMinSizeSparseSet<Func, 0, false, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherTargetComponentTypes...>(std::get<SparseSet<TargetComponentType>&>(mSparseSets).size(), TypeHashGenerator::id<TargetComponentType>()));
            //}
        }

        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            //if constexpr (sizeof...(OtherTargetComponentTypes) == 0)
            //{
                execEachByMinSizeSparseSet<Func, 0, true, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
            //}
            //else
            //{
                //execEachByMinSizeSparseSet<Func, 0, true, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherTargetComponentTypes...>(std::get<SparseSet<TargetComponentType>&>(mSparseSets).size(), TypeHashGenerator::id<TargetComponentType>()));
            //}
        }

    private:

        //template<bool dummy = false>
        //TypeHash searchMinSizeSparseSet(const std::size_t nowMin, const TypeHash hash)
        //{
        //    return hash;
        //}

        template<typename Head, typename... Tail>
        std::size_t searchMinSize(const std::size_t nowMin) const 
        {
            auto size = std::get<SparseSet<Head>&>(mSparseSets).size();

            if constexpr (sizeof...(Tail) > 0)
            {
                if (nowMin <= size)
                {
                    return searchMinSize<Tail...>(nowMin);
                }
                else
                {
                    return searchMinSize<Tail...>(size);
                }
            }
            else
            {
                return (nowMin <= size ? nowMin : size);
            }
        }

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
                    return searchMinSizeSparseSet<Tail...>(size, TypeHashGenerator::id<Head>());
                }
            }
            else
            {
                return (nowMin <= size ? hash : TypeHashGenerator::id<Head>());
            }
        }

        template<typename Func, size_t SSIndex, bool withEntity, typename... ComponentTypes>
        void execEachByMinSizeSparseSet(Func f, TypeHash hash)
        {
            if constexpr (SSIndex < sizeof...(OtherComponentTypes) + 1)
            {
                if (auto& sparseSet = std::get<SSIndex>(mSparseSets); hash == sparseSet.getPackedTypeHash())
                {
                    for (const auto& entity : sparseSet.getDenseEntities())
                    {
                        // sparseSetでこれやっちゃだめ↓
                        //if (auto opIndex = sparseSet.getSparseIndexIfValid(entity))
                        //{
                            if constexpr (withEntity)
                            {
                                //f(entity, std::get<SparseSet<ComponentTypes>&>(mSparseSets).getBySparseIndex(*opIndex)...);
                                f(entity, std::get<SparseSet<ComponentTypes>&>(mSparseSets)[entity]...);
                            }
                            else
                            {
                                //f(std::get<SparseSet<ComponentTypes>&>(mSparseSets).getBySparseIndex(*opIndex)...);
                                f(std::get<SparseSet<ComponentTypes>&>(mSparseSets)[entity]...);
                            }
                        //}
                    }
                }
                else
                {
                    execEachByMinSizeSparseSet<Func, SSIndex + 1, withEntity, ComponentTypes...>(f, hash);
                }
            }
        }

        std::tuple<SparseSet<ComponentType>&, SparseSet<OtherComponentTypes>&...> mSparseSets;
    };
}

#endif