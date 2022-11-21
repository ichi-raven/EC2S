/*****************************************************************//**
 * @file   ISparseSet.hpp
 * @brief  ISparseSetクラスのヘッダファイル
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_ISPARSESET_HPP_
#define EC2S_ISPARSESET_HPP_

#include <vector>

#ifndef NDEBUG
#include <iostream>
#endif

#include "TypeHash.hpp"
#include "Entity.hpp"

namespace ec2s
{
    class ISparseSet
    {
    public:

        constexpr static std::size_t kTombstone = std::numeric_limits<std::uint32_t>::max();

        ISparseSet()
        {}

        virtual ~ISparseSet()
        {}

        void remove(const Entity entity)
        {
            auto index = static_cast<std::size_t>(entity & kEntityIndexMask);

            if (index >= mSparseEntities.size())
            {
                return;
            }

            const Entity sparseEntity = mSparseEntities[index];
            const std::size_t sparseIndex = static_cast<std::size_t>(sparseEntity & kEntityIndexMask);
            if (sparseIndex == kTombstone || (sparseEntity & kEntitySlotMask) != (entity & kEntitySlotMask))
            {
                return;
            }

            const Entity newSlot = ((((sparseEntity & kEntitySlotMask) >> kEntitySlotShiftWidth) + 1) << kEntitySlotShiftWidth);

            std::swap(mDenseEntities[sparseIndex], mDenseEntities.back());
            mSparseEntities[static_cast<std::size_t>(mDenseEntities[sparseIndex] & kEntityIndexMask)] = sparseEntity;

            mDenseEntities.pop_back();

            this->removePackedElement(sparseIndex);

            mSparseEntities[index] = newSlot | kEntityIndexMask;
        }

        std::size_t size()
        {
            return mDenseEntities.size();
        }

        void resizeSparseIndex(const std::size_t maxIndex)
        {
            mSparseEntities.resize(maxIndex, kTombstone);
        }

        const std::vector<Entity>& getDenseEntities() const
        {
            return mDenseEntities;
        }

        void dump()
        {
#ifndef NDEBUG
            std::cerr << "index dump (for debug) : \n";
            std::cerr << "sparse : \n";
            for (int i = 0; auto & e : mSparseEntities)
            {
                std::cerr << i++ << " : " << static_cast<std::size_t>((e & kEntitySlotMask) >> kEntitySlotShiftWidth) << " | " << static_cast<std::size_t>((e & kEntityIndexMask)) << "\n";
            }

            std::cerr << "dense : \n";
            for (int i = 0; auto & e : mDenseEntities)
            {
                std::cerr << i++ << " : " << e << "\n";
            }
#endif
        }

        constexpr virtual TypeHash getPackedTypeHash() const = 0;

    protected:

        virtual void removePackedElement(std::size_t index) = 0;

        std::vector<Entity> mSparseEntities;
        std::vector<Entity> mDenseEntities;
    };
}

#endif