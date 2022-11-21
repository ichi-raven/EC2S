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

            if (index >= mSparseEntities.size())
            {
                resizeSparseIndex(index + 1);
            }

            mSparseEntities[index] = (entity & kEntitySlotMask) | static_cast<Entity>(mPacked.size());
            mDenseEntities.emplace_back(entity);
            mPacked.emplace_back(args...);
        }

        virtual void removePackedElement(std::size_t sparseIndex) override
        {
            std::swap(mPacked[sparseIndex], mPacked.back());
            mPacked.pop_back();
        }

        void clear()
        {
            mSparseEntities.clear();
            mDenseEntities.clear();
            mPacked.clear();
        }

        void reserve(const std::size_t reserveSize)
        {
            mSparseEntities.reserve(reserveSize);
            mPacked.reserve(reserveSize);
            mDenseEntities.reserve(reserveSize);
        }

        T& operator[](const Entity entity)
        {
            auto index = static_cast<size_t>(entity & kEntityIndexMask);
            auto sparseEntity = mSparseEntities[index];
            auto sparseIndex = static_cast<std::size_t>(sparseEntity & kEntityIndexMask);

            assert(sparseIndex < mPacked.size() || !"accessed by invalid entity!");

            assert((entity & kEntitySlotMask) == (sparseEntity & kEntitySlotMask) || !"accessed by invalid(deleted) entity!");

            return mPacked[sparseIndex];
        }

        std::optional<std::size_t> getSparseIndexIfValid(const Entity entity)
        {
            auto index = static_cast<size_t>(entity & kEntityIndexMask);
            auto sparseEntity = mSparseEntities[index];
            auto sparseIndex = static_cast<std::size_t>(sparseEntity & kEntityIndexMask);

            if (sparseIndex < mPacked.size() && (entity & kEntitySlotMask) == (sparseEntity & kEntitySlotMask))
            {
                return std::make_optional<std::size_t>(sparseIndex);
            }

            return std::nullopt;
        }

        inline T& getBySparseIndex(const std::size_t sparseIndex)
        {
            return mPacked[sparseIndex];
        }

        template<typename Func, typename = std::enable_if_t<std::is_invocable_v<Func, T>>>
        void each(Func f)
        {
            for (auto& e : mPacked)
            {
                f(e);
            }
        }

        template<typename Func, typename = std::enable_if_t<std::is_invocable_v<Func, Entity, T>>>
        void each(Func f, int dummy = 0)
        {
            for (std::size_t i = 0; i < mPacked.size(); ++i)
            {
                f(mDenseEntities[i], mPacked[i]);
            }
        }

        constexpr TypeHash getPackedTypeHash() const
        {
            return TypeHashGenerator::id<T>();
        }

    private:

        std::vector<T> mPacked;
    };
}

#endif