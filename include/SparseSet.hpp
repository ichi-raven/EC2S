/*****************************************************************//**
 * @file   SparseSet.hpp
 * @brief  SparseSetクラスのヘッダファイル
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_SPARSESET_HPP_
#define EC2S_SPARSESET_HPP_

#include "ISparseSet.hpp"
#include "Traits.hpp"

#include <cassert>
#include <optional>

namespace ec2s
{
    template<typename T>
    class SparseSet : public ISparseSet
    {
    public:

        SparseSet()
        {}

        virtual ~SparseSet() override
        {}

        template<typename... Args>
        void emplace(Entity entity, Args... args)
        {
            auto index = static_cast<std::size_t>(entity & kEntityIndexMask);

            if (index >= mSparseIndices.size())
            {
                resizeSparseIndex(index + 1);
            }

            mSparseIndices[index] = mPacked.size();
            mDenseEntities.emplace_back(entity);
            mPacked.emplace_back(args...);
        }

        void reserve(const std::size_t reserveSize)
        {
            mSparseIndices.reserve(reserveSize);
            mPacked.reserve(reserveSize);
            mDenseEntities.reserve(reserveSize);
        }

        T& operator[](const Entity entity)
        {
            auto index = static_cast<size_t>(entity & kEntityIndexMask);
            assert(index < mSparseIndices.size() || !"accessed by invalid entity!");
            auto sparseIndex = mSparseIndices[index];

            assert(sparseIndex < mPacked.size() || !"accessed by invalid(index over) entity!");

            assert((entity & kEntitySlotMask) == (mDenseEntities[sparseIndex] & kEntitySlotMask) || !"accessed by invalid(deleted) entity!");

            return mPacked[sparseIndex];
        }

        bool getSparseIndexIfValid(const Entity entity, std::size_t& sparseIndex_out)
        {
            auto index = static_cast<size_t>(entity & kEntityIndexMask);
            std::size_t sparseIndex = 0;
            if (index >= mSparseIndices.size() || (sparseIndex = mSparseIndices[index]) >= mPacked.size())
            {
                sparseIndex_out = 0;
                return false;
            }

            sparseIndex_out = sparseIndex;
            return true;
        }

        T& getBySparseIndex(std::size_t sparseIndex, [[maybe_unused]] const Entity entity)
        {
            assert((entity & kEntitySlotMask) == (mDenseEntities[sparseIndex] & kEntitySlotMask) || !"accessed by invalid(deleted) entity!");

            return mPacked[sparseIndex];
        }

        template<typename Func, typename Traits::IsEligibleEachFunc<Func, T>* = nullptr >
        void each(Func func)
        {
            for (auto& e : mPacked)
            {
                func(e);
            }
        }

        template<typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, T>* = nullptr >
        void each(Func func)
        {
            for (std::size_t i = 0; i < mPacked.size(); ++i)
            {
                func(mDenseEntities[i], mPacked[i]);
            }
        }

        constexpr TypeHash getPackedTypeHash() const
        {
            return TypeHasher::hash<T>();
        }

    private:
        virtual void removePackedElement(std::size_t sparseIndex) override
        {
            std::swap(mPacked[sparseIndex], mPacked.back());
            mPacked.pop_back();
        }
        
        virtual void clearPackedElement() override
        {
            mPacked.clear();
        }


        std::vector<T> mPacked;
    };
}

#endif