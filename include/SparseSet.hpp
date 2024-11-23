/*****************************************************************//**
 * @file   SparseSet.hpp
 * @brief  header file of SparseSet class
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
    /**
     * @brief  type specific implementation of SparseSet
     * 
     * @tparam T component type
     */
    template<typename T>
    class SparseSet : public ISparseSet
    {
    public:

        /** 
         * @brief  constructor
         *  
         */
        SparseSet()
        {}

        /** 
         * @brief  destructor
         *  
         */
        virtual ~SparseSet() override
        {}

        /** 
         * @brief  adds an element to the specified Entity
         *  
         * @tparam Args types of arguments forwarded to the Component constructor
         * @param entity entity to be added an element
         * @param ...args arguments forwarded to the Component constructor
         */
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
         *  
         * @param entity entity as index
         * @return reference to the element
         */
        T& operator[](const Entity entity)
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
         * @tparam IsEligibleEachFunc Trait to determine if the Func type is correctly callable for the specified Component type
         * @param func system function
         */
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, T>* = nullptr >
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
        template<typename Func, typename Traits::IsEligibleEachFunc<Func, Entity, T>* = nullptr >
        void each(Func func)
        {
            for (std::size_t i = 0; i < mPacked.size(); ++i)
            {
                func(mDenseEntities[i], mPacked[i]);
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

        //! actual element's vector
        std::vector<T> mPacked;
    };
}

#endif