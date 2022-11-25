/*****************************************************************//**
 * @file   View.hpp
 * @brief  View�N���X�̃w�b�_�t�@�C��
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
     * @brief  View�N���X�CRegistry���琶�����C����Component�ɑ΂���each��񋟂���
     * @tparam ComponentType ����View���Q�Ƃ���ComponentData�̌^(�Œ�1��)
     * @tparam OtherComponentTypes ����Component�^�p
     */
    template<typename ComponentType, typename... OtherComponentTypes>
    class View
    {
    public:

        /** 
         * @brief  �R���X�g���N�^
         * @details ���[�U�͎����ł�����\�z����K�v�͂Ȃ�
         * @param head 1�ڂ�ComponentType��SparseSet�̎Q��
         * @param tails �ȍ~��ComponentType��SparseSet�̎Q��
         */
        View(SparseSet<ComponentType>& head, SparseSet<OtherComponentTypes>&... tails)
            : mSparseSets(head, tails...)
        {}

        /**
         * @brief  �Q�Ƃ��Ă���SparseSet�̒��ōł��v�f�������Ȃ����̗̂v�f����Ԃ�
         * @details �܂�each()�͂��̉񐔂������s�����
         */
        std::size_t getMinSize() const
        {
            return searchMinSize<OtherComponentTypes...>(std::get<0>(mSparseSets).size());
        }

        /**
         * @brief  �Q�Ƃ��Ă���Component�S�Ăɑ΂���func�����s����
         * @tparam Func func�̌^(���_�����)
         * @tparam Traits::IsEligibleEachFunc ���[�U�͂����n���K�v�͂Ȃ�, Func�^���Q�Ƃ��Ă���Component�������Ƃ��Ď󂯎���Ď��s�\���ǂ���
         * @param func ���s�����֐��I�u�W�F�N�g�C�����_�����̑�
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, false, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /**
         * @brief  �Q�Ƃ��Ă���Component�S�Ăɑ΂��āC����Entity�ƂƂ���func�����s����
         * @tparam Func func�̌^(���_�����)
         * @tparam Traits::IsEligibleEachFunc ���[�U�͂����n���K�v�͂Ȃ�, Func�^��Entity�y�юQ�Ƃ��Ă���Component�������Ƃ��Ď󂯎���Ď��s�\���ǂ���
         * @param func ���s�����֐��I�u�W�F�N�g�C�����_�����̑�
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, ComponentType, OtherComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, true, ComponentType, OtherComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /** 
         * @brief  View���Q�Ƃ��Ă���Component�̂����w�肵�����̂ɂ���func�����s����
         * @tparam TargetComponentType �w�肷��Component�^�̐擪(1�ȏ�)
         * @tparam OtherTargetComponentTypes �ȍ~��Component�^
         * @tparam Func func�̌^(���_�����)
         * @tparam Traits::IsEligibleEachFunc ���[�U�͂����n���K�v�͂Ȃ�, Func�^��Entity�y�юQ�Ƃ��Ă���Component�������Ƃ��Ď󂯎���Ď��s�\���ǂ���
         * @param  func ���s�����֐��I�u�W�F�N�g�C�����_�����̑�
         */
        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, false, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

        /**
         * @brief  View���Q�Ƃ��Ă���Component�̂����w�肵�����̂ɂ���func�����s����
         * @tparam TargetComponentType �w�肷��Component�^�̐擪(1�ȏ�)
         * @tparam OtherTargetComponentTypes �ȍ~��Component�^
         * @tparam Func func�̌^(���_�����)
         * @tparam Traits::IsEligibleEachFunc ���[�U�͂����n���K�v�͂Ȃ�, Func�^��Entity�y�юQ�Ƃ��Ă���Component�������Ƃ��Ď󂯎���Ď��s�\���ǂ���
         * @param  func ���s�����֐��I�u�W�F�N�g�C�����_�����̑�
         */
        template<typename TargetComponentType, typename... OtherTargetComponentTypes, typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, TargetComponentType, OtherTargetComponentTypes...>* = nullptr>
        void each(Func func)
        {
            invokeByMinSizeSparseSet<Func, 0, true, TargetComponentType, OtherTargetComponentTypes...>(func, searchMinSizeSparseSet<OtherComponentTypes...>(std::get<0>(mSparseSets).size(), TypeHasher::hash<ComponentType>()));
        }

    private:

        /**
         * @brief  mSparseSets�^�v���̒�����ł��������T�C�Y��SparseSet��T���C���̃T�C�Y��Ԃ�
         */
        template<typename Head, typename... Tail>
        std::size_t searchMinSize(const std::size_t nowMin) const 
        {
            return std::min(std::get<SparseSet<Head>&>(mSparseSets).size(), std::get<SparseSet<Tail>&>(mSparseSets).size()...);
        }

        /**
         * @brief  mSparseSets�^�v���̒�����ł��������T�C�Y��SparseSet��T���C����SparseSet�̌^�̃n�b�V���l��Ԃ�
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
         * @brief  �ŏ��T�C�Y��SparseSet���n�b�V���l���猩���C����DenseEntities�ɑ΂���invokeIfValidEntity�����s����
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
         * @brief  �w�肵���eSparseSet�ɑ΂��āCentities�̂����S�ĂŗL��������entity��func�����s����
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