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
    /**
     * @brief  Viewクラス，Registryから生成し，複数Componentに対するeachを提供する
     * @tparam ComponentType このViewが参照するComponentDataの型(最低1つ)
     * @tparam OtherComponentTypes 複数Component型用
     */
    template<typename ComponentType, typename... OtherComponentTypes>
    class View
    {
    public:

        /** 
         * @brief  コンストラクタ
         * @details ユーザは自分でこれを構築する必要はない
         * @param head 1つ目のComponentTypeのSparseSetの参照
         * @param tails 以降のComponentTypeのSparseSetの参照
         */
        View(SparseSet<ComponentType>& head, SparseSet<OtherComponentTypes>&... tails)
            : mSparseSets(head, tails...)
        {}

        /**
         * @brief  参照しているSparseSetの中で最も要素数が少ないものの要素数を返す
         * @details つまりeach()はこの回数だけ実行される
         */
        std::size_t getMinSize() const
        {
            return searchMinSize<OtherComponentTypes...>(std::get<0>(mSparseSets).size());
        }

        /**
         * @brief  参照しているComponent全てに対してfuncを実行する
         * @tparam Func funcの型(推論される)
         * @tparam Traits::IsEligibleEachFunc ユーザはこれを渡す必要はない, Func型が参照しているComponentを引数として受け取って実行可能かどうか
         * @param func 実行される関数オブジェクト，ラムダ式その他
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, false, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /**
         * @brief  参照しているComponent全てに対して，そのEntityとともにfuncを実行する
         * @tparam Func funcの型(推論される)
         * @tparam Traits::IsEligibleEachFunc ユーザはこれを渡す必要はない, Func型がEntity及び参照しているComponentを引数として受け取って実行可能かどうか
         * @param func 実行される関数オブジェクト，ラムダ式その他
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, true, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /** 
         * @brief  Viewが参照しているComponentのうち指定したものについてfuncを実行する
         * @tparam TargetComponentType 指定するComponent型の先頭(1つ以上)
         * @tparam OtherTargetComponentTypes 以降のComponent型
         * @tparam Func funcの型(推論される)
         * @tparam Traits::IsEligibleEachFunc ユーザはこれを渡す必要はない, Func型がEntity及び参照しているComponentを引数として受け取って実行可能かどうか
         * @param  func 実行される関数オブジェクト，ラムダ式その他
         */
        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, false, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /**
         * @brief  Viewが参照しているComponentのうち指定したものについてfuncを実行する
         * @tparam TargetComponentType 指定するComponent型の先頭(1つ以上)
         * @tparam OtherTargetComponentTypes 以降のComponent型
         * @tparam Func funcの型(推論される)
         * @tparam Traits::IsEligibleEachFunc ユーザはこれを渡す必要はない, Func型がEntity及び参照しているComponentを引数として受け取って実行可能かどうか
         * @param  func 実行される関数オブジェクト，ラムダ式その他
         */
        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, true, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

    private:

        /**
         * @brief  mSparseSetsタプルの中から最も小さいサイズのSparseSetを探し，そのサイズを返す
         */
        template<typename Head, typename... Tail>
        std::size_t searchMinSize(const std::size_t nowMin) const 
        {
            return std::min(std::get<SparseSet<Head>&>(mSparseSets).size(), std::get<SparseSet<Tail>&>(mSparseSets).size()...);
        }

        /**
         * @brief  mSparseSetsタプルの中から最も小さいサイズのSparseSetを探し，そのSparseSetの型のハッシュ値を返す
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
         * @brief  最小サイズのSparseSetをハッシュ値から見つけ，そのDenseEntitiesに対してinvokeIfValidEntityを実行する
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
         * @brief  指定した各SparseSetに対して，entitiesのうち全てで有効だったentityでfuncを実行する
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
                    func(entity, std::get<I>(mSparseSets).getBySparseIndex(sparseIndices[I], entity)...);
                }
                else
                {
                    func(std::get<I>(mSparseSets).getBySparseIndex(sparseIndices[I], entity)...);
                }
            }
        }

        std::tuple<SparseSet<ComponentType>&, SparseSet<OtherComponentTypes>&...> mSparseSets;
    };
}

#endif