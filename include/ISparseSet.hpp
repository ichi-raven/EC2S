/*****************************************************************/ /**
 * @file   ISparseSet.hpp
 * @brief  header file of ISparseSet class
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
        {
        }

        virtual ~ISparseSet()
        {
        }

        void remove(const Entity entity)
        {
            auto index = static_cast<std::size_t>(entity & kEntityIndexMask);

            if (index >= mSparseIndices.size())
            {
                return;
            }

            const std::size_t sparseIndex = mSparseIndices[index];
            if (sparseIndex == kTombstone || (mDenseEntities[sparseIndex] & kEntitySlotMask) != (entity & kEntitySlotMask))
            {
                return;
            }

            std::swap(mDenseEntities[sparseIndex], mDenseEntities.back());
            mSparseIndices[static_cast<std::size_t>(mDenseEntities[sparseIndex] & kEntityIndexMask)] = sparseIndex;

            mDenseEntities.pop_back();

            this->removePackedElement(sparseIndex);

            mSparseIndices[index] = kTombstone;
        }

        void clear()
        {
            mSparseIndices.clear();
            mDenseEntities.clear();

            this->clearPackedElement();
        }

        bool contains(const Entity entity)
        {
            const auto index = static_cast<std::size_t>(entity & kEntityIndexMask);

            if (index >= mSparseIndices.size())
            {
                return false;
            }

            const std::size_t sparseIndex = mSparseIndices[index];
            
            return sparseIndex != kTombstone && (mDenseEntities[sparseIndex] & kEntitySlotMask) == (entity & kEntitySlotMask);
        }

        std::size_t size() const
        {
            return mDenseEntities.size();
        }

        void resizeSparseIndex(const std::size_t maxIndex)
        {
            mSparseIndices.resize(maxIndex, kTombstone);
        }

        const std::vector<Entity>& getDenseEntities() const
        {
            return mDenseEntities;
        }

        void dump()
        {
#ifndef NDEBUG
            std::cerr << "index dump : \n";
            std::cerr << "sparse : \n";
            for (int i = 0; auto& e : mSparseIndices)
            {
                std::cerr << i++ << " : " << e << "\n";
            }

            std::cerr << "dense : \n";
            for (int i = 0; auto& e : mDenseEntities)
            {
                std::cerr << i++ << " : " << static_cast<std::size_t>((e & kEntitySlotMask) >> kEntitySlotShiftWidth) << " | " << static_cast<std::size_t>((e & kEntityIndexMask)) << "\n";
            }
#endif
        }

    protected:
        virtual void removePackedElement(std::size_t index) = 0;

        virtual void clearPackedElement() = 0;

        std::vector<std::size_t> mSparseIndices;
        std::vector<Entity> mDenseEntities;
    };
}  // namespace ec2s

#endif