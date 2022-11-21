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

        template<typename Func>
        void each(Func f)
        {
            // 最もサイズの小さいSparseSetのDenseIndicesをイテレート
            execEachByMinSizeSparseSet<Func, 0, ComponentType, OtherComponentTypes...>(f, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
        }

        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func>
        void each(Func f)
        {
            if constexpr (sizeof...(OtherTargetComponentTypes) == 0)
            {
                execEachByMinSizeSparseSet<Func, 0, TargetComponentType, OtherTargetComponentTypes...>(f, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
            }
            else
            {
                execEachByMinSizeSparseSet<Func, 0, TargetComponentType, OtherTargetComponentTypes...>(f, searchMinSizeSparseSet<OtherTargetComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHashGenerator::id<ComponentType>()));
            }
        }

    private:

        template<size_t dummy = 0>
        TypeHash searchMinSizeSparseSet(const std::size_t nowMin, const TypeHash hash)
        {
            return hash;
        }

        template<typename Head, typename... Tail>
        TypeHash searchMinSizeSparseSet(const std::size_t nowMin, const TypeHash hash)
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

            return (nowMin <= size ? hash : TypeHashGenerator::id<Head>());
        }

        template<typename Func, size_t SSIndex, typename... ComponentTypes>
        void execEachByMinSizeSparseSet(Func f, TypeHash hash)
        {
            if constexpr (SSIndex < sizeof...(OtherComponentTypes) + 1)
            {
                if (auto& sparseSet = std::get<SSIndex>(mSparseSets); hash == sparseSet.getPackedTypeHash())
                {
                    auto& entities = sparseSet.getDenseEntities();
                    for (const auto& entity : entities)
                    {
                        if (auto opIndex = sparseSet.getSparseIndexIfValid(entity))
                        {
                            f(std::get<SparseSet<ComponentTypes>&>(mSparseSets).getBySparseIndex(*opIndex)...);
                        }
                    }
                }
                else
                {
                    execEachByMinSizeSparseSet<Func, SSIndex + 1, ComponentTypes...>(f, hash);
                }
            }
        }

        std::tuple<SparseSet<ComponentType>&, SparseSet<OtherComponentTypes>&...> mSparseSets;
    };
}

#endif