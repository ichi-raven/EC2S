/*****************************************************************/ /**
 * @file   ArenaAllocator.hpp
 * @brief  header file of ArenaAllocator class
 * 
 * @author ichi-raven
 * @date   February 2026
 *********************************************************************/
#ifndef EC2S_ARENA_ALLOCATOR_HPP_
#define EC2S_ARENA_ALLOCATOR_HPP_

#include <algorithm>
#include <cstddef>

namespace ec2s
{
    template <size_t kBlockSize = 256>
    class ArenaAllocator
    {
    public:
        ArenaAllocator(const size_t byteSize) noexcept
            : mpExternalMemory(nullptr)
            , mExternalMemorySize(0)
        {
            reallocate(byteSize);
        }

        ArenaAllocator(void* pMemory, const size_t memoryByteSize) noexcept
            : mpExternalMemory(pMemory)
            , mExternalMemorySize(memoryByteSize)
        {
            reallocate(memoryByteSize);
        }

        ArenaAllocator(const ArenaAllocator&)            = delete;
        ArenaAllocator& operator=(const ArenaAllocator&) = delete;

        ArenaAllocator(ArenaAllocator&& other) noexcept
        {
            if (this != &other)
            {
                while (mpHead)
                {
                    delete[] mpHead->pArena;
                    void* tmp = mpHead;
                    mpHead    = mpHead->pNext;
                    delete tmp;
                }
            }

            mpHead       = other.mpHead;
            other.mpHead = nullptr;
        }

        ArenaAllocator& operator=(ArenaAllocator&& other) noexcept
        {
            if (this != &other)
            {
                while (mpHead)
                {
                    delete[] mpHead->pArena;
                    void* tmp = mpHead;
                    mpHead    = mpHead->pNext;
                    delete tmp;
                }
            }

            mpHead       = other.mpHead;
            other.mpHead = nullptr;
            return *this;
        }

        ~ArenaAllocator() noexcept
        {
            while (mpHead)
            {
                delete[] mpHead->pArena;
                void* tmp = mpHead;
                mpHead    = mpHead->pNext;
                delete tmp;
            }
        }

        void* allocate(const size_t numBytes) noexcept
        {
            if (mpHead->pCurrentPtr + numBytes > mpHead->pArena + mpHead->size)
            {
                if (!reallocate(std::max(kBlockSize, numBytes)))
                {
                    // failed to allocate new block
                    return nullptr;
                }
            }
            void* rtn = mpHead->pCurrentPtr;
            mpHead->pCurrentPtr += numBytes;
            return rtn;
        }

        template <typename T>
        T* allocate(const size_t num = 1) noexcept
        {
            const size_t allocSize = sizeof(T) * num;

            return reinterpret_cast<T*>(allocate(allocSize));
        }

        void reset()
        {
            while (mpHead)
            {
                mpHead->pCurrentPtr = mpHead->pArena;
                mpHead              = mpHead->pNext;
            }
        }

    private:

        bool reallocate(const size_t newBlockSize) noexcept
        {
            auto* prevHead = mpHead;

            if (mpExternalMemory)
            {
                mpHead         = new (std::nothrow) MemoryBlock();
                mpHead->pArena = static_cast<std::byte*>(mpExternalMemory);
                if (mpHead == nullptr || mpHead->pArena == nullptr)
                {
                    // failed to allocate new block
                    mpHead = prevHead;
                    return false;
                }
            }
            else
            {
                mpHead         = new (std::nothrow) MemoryBlock();
                mpHead->pArena = new (std::nothrow) std::byte[newBlockSize];

                if (mpHead == nullptr || mpHead->pArena == nullptr)
                {
                    // failed to allocate new block
                    mpHead = prevHead;
                    return false;
                }
            }

            mpHead->pCurrentPtr = mpHead->pArena;
            mpHead->size        = newBlockSize;
            mpHead->pNext       = prevHead;
        }

        struct MemoryBlock
        {
            std::byte* pArena;
            std::byte* pCurrentPtr;
            size_t size;
            MemoryBlock* pNext = nullptr;
        };

        MemoryBlock* mpHead;
        void* mpExternalMemory;
        size_t mExternalMemorySize;
    };

}  // namespace ec2s

#endif