/*****************************************************************/ /**
 * @file   ArenaAllocator.hpp
 * @brief  header file of ArenaAllocator class
 * 
 * @author ichi-raven
 * @date   February 2026
 *********************************************************************/
#ifndef EC2S_ARENA_ALLOCATOR_HPP_
#define EC2S_ARENA_ALLOCATOR_HPP_

#include <cstddef>

namespace ec2s
{
    template <size_t kCacheLineSize = 128>
    class ArenaAllocator
    {
    public:
        ArenaAllocator(const size_t byteSize) noexcept
        {
            mpHead              = new MemoryBlock();
            mpHead->pArena      = new std::byte[byteSize];
            mpHead->pCurrentPtr = mpHead->pArena;
            mpHead->size        = byteSize;
            mpHead->pNext       = nullptr;
        }

        ArenaAllocator(std::byte* pArena, const size_t memSize) noexcept
        {
            mpHead              = new MemoryBlock();
            mpHead->pArena      = pArena;
            mpHead->pCurrentPtr = mpHead->pArena;
            mpHead->size        = memSize;
            mpHead->pNext       = nullptr;
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

        template <typename T>
        T* allocate(const size_t num = 1) noexcept
        {
            const size_t allocSize = sizeof(T) * num;

            if (mpHead->pCurrentPtr + allocSize > mpHead->pArena + mpHead->size)
            {
                reallocate(std::max(kCacheLineSize, allocSize));
            }

            T* rtn = reinterpret_cast<T*>(mpHead->pCurrentPtr);
            mpHead->pCurrentPtr += allocSize;
            return rtn;
        }

        //void deallocate(void* ptr) noexcept
        //{
        //    // do nothing, memory will be released when the allocator is destroyed
        //}

    private:
        void reallocate(const size_t newBlockSize) noexcept
        {
            const auto* prevHead = mpHead;

            mpHead               = new MemoryBlock();
            mpHead->pArena       = new std::byte[newBlockSize];
            mpHead->pCurrentPtr  = mpHead->pArena;
            mpHead->size         = newBlockSize;
            mpHead->pNext        = prevHead;
        }

        struct MemoryBlock
        {
            std::byte* pArena;
            std::byte* pCurrentPtr;
            size_t size;
            MemoryBlock* pNext = nullptr;
        };

        MemoryBlock* mpHead;
    };

}  // namespace ec2s

#endif