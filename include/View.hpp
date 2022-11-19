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
    template<typename... Args>
    class View
    {
    public:
        View(SparseSet<Args>&... sets)
            : mSparseSets(sets...)
        {

        }

        template<typename Func>
        void each(Func f)
        {
            // 最もサイズの小さいSparseSetのDenseIndicesをイテレート
            execEachByMinSizeSparseSet<Func, Args...>(searchMinSizeSparseSet<Args...>(std::numeric_limits<std::size_t>::max()), f);
        }

        /*template<typename T, typename Func>
        void each(Func f)
        {
            std::get<SparseSet<T>&>(mSparseSets).each(f);
        }*/

        template<typename T1, typename Func, typename... Tail>
        void each(Func f)
        {
            execEachByMinSizeSparseSet<Func, T1, Tail...>(searchMinSizeSparseSet<T1, Tail...>(std::numeric_limits<std::size_t>::max()), f);
        }

    private:

        template<typename Head, typename... Tail>
        std::size_t searchMinSizeSparseSet(const std::size_t nowMin)
        {
            auto&& size = std::get<SparseSet<Head>&>(mSparseSets).size();

            if constexpr (sizeof...(Tail) > 0)
            {
                return searchMinSizeSparseSet<Tail...>((nowMin <= size ? nowMin : size));
            }

            return (nowMin <= size ? nowMin : size);
        }

        template<typename Func, typename Head, typename... Tail>
        void execEachByMinSizeSparseSet(std::size_t minSize, Func f)
        {
            auto& sparseSet = std::get<SparseSet<Head>&>(mSparseSets);

            if (minSize == sparseSet.size())
            {
                for (const auto& entity : sparseSet.getDenseIndices())
                {
                    f(std::get<SparseSet<Args>&>(mSparseSets)[entity]...);
                }
            }
            else
            {
                if constexpr (sizeof...(Tail) > 0)
                {
                    execEachByMinSizeSparseSet<Func, Tail...>(minSize, f);
                }
            }
        }

        std::tuple<SparseSet<Args>&...> mSparseSets;
    };
}

#endif