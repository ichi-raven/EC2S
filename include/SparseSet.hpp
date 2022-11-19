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

#include <cassert>

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
        void emplace(std::size_t index, Args... args)
        {
            if (index >= mSparseIndices.size())
            {
                resizeSparseIndex(index + 1);
            }

            mSparseIndices[index] = mPacked.size();
            mDenseIndices.emplace_back(index);
            mPacked.emplace_back(args...);
        }

        virtual void removePackedElement(std::size_t index) override
        {
            std::swap(mPacked[mSparseIndices[index]], mPacked.back());
            mPacked.pop_back();
        }

        void clear()
        {
            mSparseIndices.clear();
            mDenseIndices.clear();
            mPacked.clear();
        }

        void reserve(const std::size_t reserveSize)
        {
            mPacked.reserve(reserveSize);
            mDenseIndices.reserve(reserveSize);
        }

        T& operator[](const std::size_t index)
        {
            assert(mSparseIndices[index] < mPacked.size());
            return mPacked[mSparseIndices[index]];
        }

        template<typename Func>
        void each(Func f)
        {
            for (auto& e : mPacked)
            {
                f(e);
            }
        }

        constexpr virtual TypeHash getPackedTypeHash() const
        {
            return TypeHashGenerator<T>::id();
        }

    private:

        std::vector<T> mPacked;
    };
}

#endif