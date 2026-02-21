/*****************************************************************/ /**
 * @file   ISparseSet.hpp
 * @brief  header file of ISparseSet class
 * 
 * @author ichi-raven
 * @date   November 2022
 *********************************************************************/
#ifndef EC2S_ISPARSESET_HPP_
#define EC2S_ISPARSESET_HPP_

#include <optional>
#include <memory_resource>

#ifndef NDEBUG
#include <sstream>
#endif

#include "Concepts.hpp"
#include "TypeHash.hpp"
#include "Entity.hpp"

#include <string>

namespace ec2s
{
    /**
     * @brief  interface to Sparse Set container class (to change the process depending on the concrete element type)
     */
    class ISparseSet
    {
    public:
        //! represents invalid index
        constexpr static std::size_t kTombstone = std::numeric_limits<Entity>::max();

        /** 
         * @brief  constructor
         *  
         */
        ISparseSet()
        {
        }

        /** 
         * @brief  constructor with external memory resource
         *  
         */
        ISparseSet(std::pmr::memory_resource* const pMemoryResource)
            : mSparseIndices(pMemoryResource)
            , mDenseEntities(pMemoryResource)
        {
        }

        /** 
         * @brief  destructor (virtual)
         *  
         */
        virtual ~ISparseSet()
        {
        }

        /** 
         * @brief  remove the element from specified entity
         *  
         * @param entity entity that removes element
         */
        void remove(const Entity entity)
        {
            auto index = getEntityIndex<std::size_t>(entity);

            if (index >= mSparseIndices.size())
            {
                return;
            }

            const std::size_t sparseIndex = mSparseIndices[index];
            if (sparseIndex == kTombstone || getEntitySlot(mDenseEntities[sparseIndex]) != getEntitySlot(entity))
            {
                return;
            }

            std::swap(mDenseEntities[sparseIndex], mDenseEntities.back());
            mSparseIndices[getEntityIndex<std::size_t>(mDenseEntities[sparseIndex])] = sparseIndex;
            mDenseEntities.pop_back();

            // destruct elements
            this->removePackedElement(sparseIndex);

            // clear index
            mSparseIndices[index] = kTombstone;
        }

        /**
         * @brief swap the elements corresponding to the specified entities
         * 
         * @param left left entity
         * @param right right entity
         */
        void swap(const Entity left, const Entity right)
        {
            auto leftIndex  = getEntityIndex<std::size_t>(left);
            auto rightIndex = getEntityIndex<std::size_t>(right);
            if (leftIndex >= mSparseIndices.size() || rightIndex >= mSparseIndices.size())
            {
                return;
            }
            auto leftSparseIndex  = mSparseIndices[leftIndex];
            auto rightSparseIndex = mSparseIndices[rightIndex];
            if (leftSparseIndex == kTombstone || rightSparseIndex == kTombstone || getEntitySlot(mDenseEntities[leftSparseIndex]) != getEntitySlot(left) || getEntitySlot(mDenseEntities[rightSparseIndex]) != getEntitySlot(right))
            {
                return;
            }

            std::swap(mDenseEntities[leftSparseIndex], mDenseEntities[rightSparseIndex]);
            this->swapPackedElements(leftSparseIndex, rightSparseIndex);

            mSparseIndices[leftIndex]  = rightSparseIndex;
            mSparseIndices[rightIndex] = leftSparseIndex;
        }

        /** 
         * @brief  clear all indices and elements
         *  
         */
        void clear()
        {
            mSparseIndices.clear();
            mDenseEntities.clear();

            // destruct elements
            this->clearPackedElement();
        }

        /** 
         * @brief  checks if the specified Entity has been included
         *  
         * @param entity entity to be checked 
         * @return whether the entity has been included
         */
        bool contains(const Entity entity)
        {
            const auto index = getEntityIndex<std::size_t>(entity);

            if (index >= mSparseIndices.size())
            {
                return false;
            }

            const std::size_t sparseIndex = mSparseIndices[index];

            return sparseIndex != kTombstone && getEntitySlot(mDenseEntities[sparseIndex]) == getEntitySlot(entity);
        }

        /** 
         * @brief  returns the actual number of elements
         *  
         * @return size
         */
        std::size_t size() const
        {
            return mDenseEntities.size();
        }

        /** 
         * @brief  resize sparseIndices
         *  
         * @param maxIndex
         */
        void resizeSparseIndex(const std::size_t maxIndex)
        {
            mSparseIndices.resize(maxIndex, kTombstone);
        }

        /** 
         * @brief  return reference to denseEntities
         *  
         * @return reference to denseEntities
         */
        const std::pmr::vector<Entity>& getDenseEntities() const
        {
            return mDenseEntities;
        }

        /** 
         * @brief  dump whole container internals as a string
         * @return dumped result string
         */
        std::string dump()
        {
#ifndef NDEBUG
            std::ostringstream oss;
            oss << "index dump : \n";
            oss << "sparse : \n";
            for (int i = 0; auto& e : mSparseIndices)
            {
                oss << i++ << " : " << e << "\n";
            }

            oss << "dense : \n";
            for (int i = 0; auto& e : mDenseEntities)
            {
                oss << i++ << " : " << static_cast<std::size_t>((e & kEntitySlotMask) >> kEntitySlotShiftWidth) << " | " << static_cast<std::size_t>((e & kEntityIndexMask)) << "\n";
            }

            return oss.str();
#endif
            return "";
        }

    protected:
        /** 
         * @brief  type-dependent implementation of element destruction (left to child classes)
         *  
         * @param index index of senseIndices to be deleted
         */
        virtual void removePackedElement(std::size_t index) = 0;

        /** 
         * @brief  type-dependent implementation of all element destruction (left to child classes)
         *  
         */
        virtual void clearPackedElement() = 0;

        /**
         *  @ type-dependent implementation of swapping two elements (left to child classes) 
         * 
         * @param leftIndex dense index of left element
         * @param rightIndex dense index of right element
         */
        virtual void swapPackedElements(const std::size_t leftIndex, const std::size_t rightIndex) = 0;

        //! sparse index to DenseEntities (mapping from Entity to DenseEntities)
        std::pmr::vector<std::size_t> mSparseIndices;
        //! actual dense Entity
        std::pmr::vector<Entity> mDenseEntities;
    };
}  // namespace ec2s

#endif