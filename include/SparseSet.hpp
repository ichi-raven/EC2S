/*****************************************************************/ /**
 * @file   SparseSet.hpp
 * @brief  header file of SparseSet class
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_SPARSESET_HPP_
#define EC2S_SPARSESET_HPP_

#include "ISparseSet.hpp"
#include "Concepts.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <optional>

namespace ec2s
{
    /**
     * @brief  type specific implementation of SparseSet
     * 
     * @tparam T component type
     */
    template <typename T, typename ComponentAllocator>  // WARN: default allocator is already defined in "Condition.hpp" for forward declaration
        requires Concepts::AllocatorConcept<ComponentAllocator>
    class SparseSet : public ISparseSet
    {
    public:
        using ComponentType = T;

    public:
        /** 
         * @brief  constructor
         *  
         */
        SparseSet()
        {
        }

        /** 
         * @brief  constructor
         *  
         */
        SparseSet(ComponentAllocator& alloc)
            : mPacked(alloc)
        {
        }

        /** 
         * @brief  destructor
         *  
         */
        virtual ~SparseSet() override
        {
        }

        /** 
         * @brief  adds an element to the specified Entity
         *  
         * @tparam Args types of arguments forwarded to the Component constructor
         * @param entity entity to be added an element
         * @param ...args arguments forwarded to the Component constructor
         */
        template <typename... Args>
        T& emplace(Entity entity, Args... args)
        {
            auto index = static_cast<std::size_t>(entity & kEntityIndexMask);

            if (index >= mSparseIndices.size())
            {
                resizeSparseIndex(index + 1);
            }

            mSparseIndices[index] = mPacked.size();
            mDenseEntities.emplace_back(entity);
            return mPacked.emplace_back(args...);
        }

        /** 
         * @brief  reserve the area of each vector
         *  
         * @param reserveSize size to be reserved
         */
        void reserve(const std::size_t reserveSize)
        {
            mSparseIndices.reserve(reserveSize);
            mPacked.reserve(reserveSize);
            mDenseEntities.reserve(reserveSize);
        }

        /** 
         * @brief  operator overload for index access to element
         * @warn   this overload does not check whether given entity is correct
         * @param entity entity as index
         * @return reference to the element
         */
        T& operator[](const Entity entity)
        {
            return mPacked[mSparseIndices[static_cast<size_t>(entity & kEntityIndexMask)]];
        }

        /** 
         * @brief  find the corresponding element from the entity ID
         * @param entity  entity ID
         * @return 
         */
        T& get(const Entity entity)
        {
            auto index = static_cast<size_t>(entity & kEntityIndexMask);
            assert(index < mSparseIndices.size() || !"accessed by invalid entity!");
            auto sparseIndex = mSparseIndices[index];

            assert(sparseIndex < mPacked.size() || !"accessed by invalid(index over) entity!");

            assert((entity & kEntitySlotMask) == (mDenseEntities[sparseIndex] & kEntitySlotMask) || !"accessed by invalid(deleted) entity!");

            return mPacked[sparseIndex];
        }

        /** 
         * @brief  if there is a sparseIndex corresponding to the entity, retrieve it and return true, otherwise return false
         *  
         * @param entity Entity for the corresponding sparseIndex
         * @param sparseIndex_out sparseIndex corresponding to the specified Entity (output)
         * @return whether there was a sparseIndex corresponding to the specified Entity
         */
        bool getSparseIndexIfValid(const Entity entity, std::size_t& sparseIndex_out)
        {
            auto index              = static_cast<size_t>(entity & kEntityIndexMask);
            std::size_t sparseIndex = 0;
            if (index >= mSparseIndices.size() || (sparseIndex = mSparseIndices[index]) >= mPacked.size())
            {
                sparseIndex_out = 0;
                return false;
            }

            sparseIndex_out = sparseIndex;
            return true;
        }

        /** 
         * @brief  find the corresponding element from the sparseIndex
         *  
         * @param sparseIndex 
         * @param entity used only in DEBUG mode
         * @return 
         */
        T& getBySparseIndex(std::size_t sparseIndex, [[maybe_unused]] const Entity entity)
        {
            assert((entity & kEntitySlotMask) == (mDenseEntities[sparseIndex] & kEntitySlotMask) || !"accessed by invalid(deleted) entity!");

            return mPacked[sparseIndex];
        }

        /** 
         * @brief  execute the specified function on all elements (system in ECS)
         *  
         * @tparam T component type
         * @tparam Func function type
         * @param func system function
         */
        template <typename Func>
            requires Concepts::Invocable<Func, T>
        void each(Func func)
        {
            for (auto& e : mPacked)
            {
                func(e);
            }
        }

        /** 
         * @brief  system that takes Entity as its first argument
         *  
         * @tparam Func function type
         * @tparam IsEligibleEachFunc Trait to determine if the Func type and Entity is correctly callable for the specified Component type
         * @param func system function
         */
        template <typename Func>
            requires Concepts::InvocableWithEntity<Func, T>
        void each(Func func)
        {
            for (std::size_t i = 0; i < mPacked.size(); ++i)
            {
                func(mDenseEntities[i], mPacked[i]);
            }
        }

        void swap(const Entity left, const Entity right)
        {
            const std::size_t leftIndex = static_cast<std::size_t>(left & kEntityIndexMask);
            const std::size_t rightIndex = static_cast<std::size_t>(right & kEntityIndexMask);
            assert((leftIndex < mSparseIndices.size() && rightIndex < mSparseIndices.size()) || !"accessed by invalid entity!");
            if (left == right)
            {
                return;
            }
            
            std::swap(mPacked[mSparseIndices[leftIndex]], mPacked[mSparseIndices[rightIndex]]);
            std::swap(mDenseEntities[mSparseIndices[leftIndex]], mDenseEntities[mSparseIndices[rightIndex]]);
            mSparseIndices[leftIndex] = mSparseIndices[rightIndex];

        }

        template <typename Predicate>
            requires Concepts::Predicate<Predicate, T>
        void sort(Predicate& predicate)
        {
            std::sort(mPacked.begin(), mPacked.end(), std::move(predicate));

            for (std::size_t pos = 0, end = mDenseEntities.size(); pos < end; ++pos)
            {
                auto curr = pos;
                auto next = mSparseIndices[static_cast<std::size_t>(mDenseEntities[curr] & kEntityIndexMask)];

                while (curr != next)
                {
                    adjust(mDenseEntities[curr], mDenseEntities[next]);
                    mSparseIndices[static_cast<std::size_t>(mDenseEntities[curr] & kEntityIndexMask)] = curr;

                    curr = next;
                    next = mSparseIndices[static_cast<std::size_t>(mDenseEntities[curr] & kEntityIndexMask)];
                }
            }
        }

        /** 
         * @brief  get the type hash of the element's type
         *  
         * @return type hash of the element's type
         */
        constexpr TypeHash getPackedTypeHash() const
        {
            return TypeHasher::hash<T>();
        }

        std::vector<T, ComponentAllocator>& getPackedVector()
        {
            return mPacked;
        }

    private:
        /** 
         * @brief  implementation of the type-dependent part of element removing
         *  
         * @param sparseIndex sparseIndex of the elements to be removed
         */
        virtual void removePackedElement(std::size_t sparseIndex) override
        {
            std::swap(mPacked[sparseIndex], mPacked.back());
            mPacked.pop_back();
        }

        /** 
         * @brief  implementation of the type-dependent part of all element clearing
         *  
         */
        virtual void clearPackedElement() override
        {
            mPacked.clear();
        }

        inline void adjust(const Entity lhs, const Entity rhs)
        {
            std::swap(mDenseEntities[mSparseIndices[static_cast<std::size_t>(lhs & kEntityIndexMask)]], mDenseEntities[mSparseIndices[static_cast<std::size_t>(rhs & kEntityIndexMask)]]);
        }

        //! actual element's vector
        std::vector<T, ComponentAllocator> mPacked;
    };
}  // namespace ec2s

#endif