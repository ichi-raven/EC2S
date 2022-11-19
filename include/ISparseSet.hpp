#ifndef CECS_ISPARSESET_HPP_
#define CECS_ISPARSESET_HPP_

#include <vector>
#include <iostream>

#include "TypeHash.hpp"

namespace ec2s
{
    class ISparseSet
    {
    public:

        constexpr static std::size_t kTombstone = std::numeric_limits<std::size_t>::max();

        ISparseSet()
        {}

        virtual ~ISparseSet()
        {}

        void remove(const std::size_t index)
        {
            if (index >= mSparseIndices.size())
            {
                return;
            }

            const auto sparseIndex = mSparseIndices[index];
            //std::cerr << "removed index : " << sparseIndex << "\n";
            if (sparseIndex == kTombstone)
            {
                return;
            }

            std::swap(mDenseIndices[sparseIndex], mDenseIndices.back());
            mSparseIndices[mDenseIndices[sparseIndex]] = sparseIndex;

            mDenseIndices.pop_back();

            this->removePackedElement(index);

            mSparseIndices[index] = kTombstone;
        }

        std::size_t size()
        {
            return mDenseIndices.size();
        }

        void resizeSparseIndex(const std::size_t maxIndex)
        {
            mSparseIndices.resize(maxIndex, kTombstone);
        }

        const std::vector<std::size_t>& getDenseIndices() const
        {
            return mDenseIndices;
        }

        void dump()
        {
            std::cerr << "index dump (for debug) : \n";
            std::cerr << "sparse : \n";
            for (int i = 0; auto & e : mSparseIndices)
            {
                std::cerr << i++ << " : " << e << "\n";
            }

            std::cerr << "dense : \n";
            for (int i = 0; auto & e : mDenseIndices)
            {
                std::cerr << i++ << " : " << e << "\n";
            }
        }

        constexpr virtual TypeHash getPackedTypeHash() const = 0;

    protected:

        virtual void removePackedElement(std::size_t index) = 0;

        std::vector<std::size_t> mSparseIndices;
        std::vector<std::size_t> mDenseIndices;
    };
}

#endif